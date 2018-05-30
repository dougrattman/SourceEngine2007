// Processes two "memstats<n>.txt" memory dumps from the game into a 'diff' text
// file which Excel will display in legible form (useful for tracking mem leaks)

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using ItemMap = std::map<std::string, float>;
using Delta = std::pair<std::string, float>;

struct Sequence {
  const char *m_name;
  std::vector<float> m_values;
  float m_maxDelta;
  float m_endDelta;
};

// The number of chains which will be output (the top N, after sorting and
// skipping)
std::size_t gNumSequencesToOutput = 16;

// The number of chains which will be skipped before output (the top N, after
// sorting)
std::size_t gNumSequencesToSkip = 0;

// If this is true, we sort chains by their maximum delta from the starting
// value, otherwise we sort by the change from the start to the end value
bool gSortByMaxChange = false;

// If this is true, we use the absolute value of deltas for sorting
// (so large negative deltas rank high as well as large position ones)
bool gSortByAbsDeltas = false;

// By default, we expect the input sequence of memstats files to be in
// chronological order and from the same play session - if true, this relaxes
// that restriction:
bool gAllowArbitraryInputSequence = false;

// Output deltas from the first value in each sequence, rather than the current
// value (outputs N-1 values instead of N)
bool gOutputDeltas = false;

// If this is true, output absolute values (by default, output values are
// relative to those in the first input file - the first value in each sequence
// is subtracted out)
bool gOutputAbsolute = false;

// Output MB instead of KB
bool gOutputMB = false;

bool GetQuotedString(std::ifstream &file, std::string &item) {
  // Skip the opening quote
  char ch;
  while (file.get(ch) && (ch != '"'))
    ;
  // Get the string
  std::getline(file, item, '"');
  // Skip the comma
  file.get(ch);

  return !file.eof();
}

std::string &CleanupName(std::string &name) {
  // Strip everything before "\src\", to make memstats files from different
  // peoples' machines compatible
  const char *pSrc = strstr(name.c_str(), "src\\");
  if (pSrc && (pSrc != name.c_str())) {
    name = pSrc;
  }
  return name;
}

bool ParseFile(char *pszFile, ItemMap *pResult) {
  std::ifstream file;

  // Is this a CSV file?
  bool bIsCSV = !!strstr(pszFile, ".csv");

  pResult->clear();
  file.open(pszFile);
  if (!file.is_open()) {
    fprintf(stderr, "Failed to open %s\n", pszFile);
    return false;
  }

  std::string item;
  // Skip the header
  if (!getline(file, item)) {
    fprintf(stderr, "%s header is missed\n", pszFile);
    return false;
  }

  float size;
  float maxEntrySize = 0;
  while (!file.eof()) {
    if (bIsCSV) {
      // Comma-separated data
      if (!GetQuotedString(file, item)) break;
    } else {
      // Tab-delimited data
      getline(file, item, '\t');
      if (!item.length()) break;
    }
    file >> size;
    maxEntrySize = std::max(maxEntrySize, size);
    pResult->insert(make_pair(CleanupName(item), size));
    getline(file, item);  // skip the end of line
  }

  // XBox 360 has 512MB of RAM, so we can tell if data is in MB or KB
  // (and it's pretty unlikely we have no allocation entries greater than
  // 512KB!)
  bool bInputDataInKB = (maxEntrySize > 512);

  // Convert the output to either KB or MB, as requested
  float multiplier = 1.0f;
  if (bInputDataInKB && gOutputMB)
    multiplier = 1.0f / 1024.0f;
  else if (!bInputDataInKB && !gOutputMB)
    multiplier = 1024.0f;

  for (auto &p1 : *pResult) {
    p1.second *= multiplier;
  }

  return pResult->size() > 0;
}

bool FillMissingEntries(std::vector<ItemMap> &items, std::size_t numItems,
                        std::size_t &numAllocations) {
  // First, generate a list of all unique allocations
  ItemMap allAllocations;
  ItemMap::const_iterator p1, p2;
  for (std::size_t i = 0; i < numItems; i++) {
    for (p1 = items[i].begin(); p1 != items[i].end(); p1++) {
      p2 = allAllocations.find(p1->first);
      if (p2 == allAllocations.end())
        allAllocations.insert(make_pair(p1->first, 0.0f));
    }
  }

  // Determine how many sequences we have in total
  numAllocations = allAllocations.size();

  // Now make sure each allocation is present in every CItemMap. Where absent,
  // assign the previous known value, and where there is no known value assign
  // zero. 'Validity' requires that a given allocation will always be present in
  // CItemMaps after the first one in which it occurs (this is what you would
  // get if the input files represent memdumps in chronological order, all from
  // the same play session).
  bool isValid = true;
  for (p1 = allAllocations.begin(); p1 != allAllocations.end(); p1++) {
    float curValue = 0.0f;
    bool foundFirstOccurrence = false;
    for (std::size_t i = 0; i < numItems; i++) {
      p2 = items[i].find(p1->first);
      if (p2 != items[i].end()) {
        // Entry already present, update current value
        curValue = p2->second;
        foundFirstOccurrence = true;
      } else {
        // Entry missing, add it (and check validity)
        items[i].insert(make_pair(p1->first, curValue));
        if (foundFirstOccurrence) isValid = false;
      }
    }
  }

  return isValid;
}

bool CompareSequenceMaxDelta(const Sequence &lhs, const Sequence &rhs) {
  return (lhs.m_maxDelta > rhs.m_maxDelta);
}

bool CompareSequenceEndDelta(const Sequence &lhs, const Sequence &rhs) {
  return (lhs.m_endDelta > rhs.m_endDelta);
}

std::vector<Sequence> CreateSequences(const std::vector<ItemMap> &items,
                                      std::size_t numItems) {
  // Create a vector of Sequence objects, each of which holds the
  // sequence of 'Allocation Size' values for each allocation
  std::vector<Sequence> sequences;

  ItemMap::const_iterator p1, p2;
  for (p1 = items[0].begin(); p1 != items[0].end(); p1++) {
    Sequence seq;
    seq.m_name = p1->first.c_str();
    seq.m_values = std::vector<float>(numItems);
    float startVal = p1->second;
    float maxDelta = 0.0f;
    float endDelta = 0.0f;
    for (std::size_t i = 0; i < numItems; i++) {
      p2 = items[i].find(seq.m_name);
      assert(p2 != items[i].end());
      if (p2 != items[i].end()) {
        seq.m_values[i] = p2->second;
        float delta = p2->second - startVal;
        if (gSortByAbsDeltas) delta = fabs(delta);
        if (delta > maxDelta) maxDelta = delta;
        endDelta = delta;
      }
    }
    seq.m_endDelta = endDelta;
    seq.m_maxDelta = maxDelta;
    sequences.push_back(seq);
  }

  const auto &compareSequence =
      gSortByMaxChange ? CompareSequenceMaxDelta : CompareSequenceEndDelta;

  // Now sort the sequences vector
  sort(sequences.begin(), sequences.end(), compareSequence);

  return std::move(sequences);
}

void Usage() {
  printf("diffmemstats is used for hunting down memory leaks\n");
  printf("\n");
  printf("  USAGE: diffmemstats [options] <file1> <file2> [<file3>, ...]\n");
  printf("\n");
  printf(
      "Input is a sequence of memstats<n>.txt files (saved from game using "
      "'mem_dump')\n");
  printf(
      "and output is a single tab-separated text file, where each line "
      "represents a\n");
  printf(
      "given allocation's size as it varies over time through the memstats "
      "sequence\n");
  printf(
      "(lines are sorted by maximum change over time - see sortend/sortmax "
      "options).\n");
  printf(
      "This text file can then be graphed in Excel using a 'stacked column' "
      "chart.\n");
  printf("\n");
  printf(
      "NOTE: input files must be in chronological order, from a SINGLE play "
      "session\n");
  printf("      (unless -allowmismatch is specified).\n");
  printf("\n");
  printf("options:\n");
  printf(
      "[-numchains:N]         the top N sequences are output (default: 16)\n");
  printf(
      "[-skipchains:M]        skip the top M sequences before output (default: "
      "0)\n");
  printf(
      "[-delta]               output deltas between adjacent values in each "
      "sequence\n");
  printf(
      "                       (the first delta for each sequence will always "
      "be zero)\n");
  printf(
      "[-absolute]            output absolute values (default is to subtract "
      "out the\n");
  printf(
      "                       first value in each sequence), overridden by "
      "'-delta'\n");
  printf(
      "[-sortend]             sort sequences by start-to-end change "
      "(default)\n");
  printf(
      "[-sortmax]             sort sequences by start-to-max-value change\n");
  printf("[-sortabs]             sort by absolute change values\n");
  printf(
      "[-allowmismatch]       don't check that the input file sequence is "
      "in\n");
  printf(
      "                       chronological order and from the same play "
      "session\n");
  printf("[-mb]                  output values in MB (default is KB)\n");
}

bool ParseOption(char *option) {
  if (option[0] != '-') return false;

  option++;

  int numChains, numRead = sscanf_s(option, "numchains:%d", &numChains);
  if (numRead == 1) {
    if (numChains >= 0) {
      gNumSequencesToOutput = numChains;
      return true;
    }
    return false;
  }

  int skipChains, numRead2 = sscanf_s(option, "skipchains:%d", &skipChains);
  if (numRead2 == 1) {
    if (skipChains >= 0) {
      gNumSequencesToSkip = skipChains;
      return true;
    }
    return false;
  }

  if (!_stricmp(option, "delta")) {
    gOutputDeltas = true;
    return true;
  }

  if (!_stricmp(option, "absolute")) {
    gOutputAbsolute = true;
    return true;
  }

  if (!_stricmp(option, "sortend")) {
    gSortByMaxChange = false;
    return true;
  }

  if (!_stricmp(option, "sortmax")) {
    gSortByMaxChange = true;
    return true;
  }

  if (!_stricmp(option, "sortabs")) {
    gSortByAbsDeltas = true;
    return true;
  }

  if (!_stricmp(option, "allowmismatch")) {
    gAllowArbitraryInputSequence = true;
    return true;
  }

  if (!_stricmp(option, "mb")) {
    gOutputMB = true;
    return true;
  }

  return false;
}

// NOTE: this app doesn't bother with little things like freeing memory
int main(int argc, char *argv[]) {
  if (argc < 3) {
    Usage();
    return EXIT_FAILURE;
  }

  // Grab options
  int numOptions = 0;
  argv++;
  while (argv[0][0] == '-') {
    if (!ParseOption(argv[0])) {
      Usage();
      return 1;
    }
    numOptions++;
    argv++;
  }

  // TODO: allow the user to pass a starting filename and have the program
  // figure out the sequence of files
  //       in that folder (using Aaron's naming scheme:
  //       <map_name>_<mmdd>_<hhmmss>_<count>.txt)

  std::size_t numFiles = std::clamp(argc - 1 - numOptions, 0, argc);
  std::vector<ItemMap> items{numFiles};
  std::vector<std::string> names{numFiles};
  for (std::size_t i = 0; i < numFiles; i++) {
    _strlwr_s(argv[0], strlen(argv[0]) + 1);
    if (!ParseFile(argv[0], &items[i])) return 1;

    // Create a label for each column of output data
    std::string name = argv[0];
    if ((name.find(".csv") == (name.length() - 4)) ||
        (name.find(".txt") == (name.length() - 4))) {
      name = name.substr(0, name.length() - 4);
    }
    names[i] = (gOutputDeltas ? "[delta] " : "[size] ") + name;
    argv++;
  }

  // Generate missing entries (i.e. make it so that each allocation
  // occurs in every CItemMap, so we have a sequence of 'numFiles' value,
  // duplicating )
  std::size_t numAllocations = 0;
  bool isValidSequence = FillMissingEntries(items, numFiles, numAllocations);
  if (!isValidSequence && !gAllowArbitraryInputSequence) {
    fprintf(
        stderr,
        "ERROR: input files did not all come from the same play session, or "
        "are in the wrong order (to allow this, specify -allowmismatch)\n");
    return 1;
  }

  // Create a vector of Sequence objects, each of which holds the sequence of
  // 'size' values for each allocation. The vector is sorted based on max change
  // from the start value, or the start-to-end change (gSortByMaxChange).
  std::vector<Sequence> sequences = CreateSequences(items, numFiles);

  // Headings
  printf("Allocation type");
  for (std::size_t i = 0; i < numFiles; i++) {
    printf("\t%s", names[i].c_str());
  }
  printf("\n");

  for (std::size_t i = gNumSequencesToSkip;
       (i < (gNumSequencesToSkip + gNumSequencesToOutput)) &&
       (i < numAllocations);
       i++) {
    const Sequence &seq = sequences.at(i);
    printf(seq.m_name);
    for (std::size_t j = 0; j < numFiles; j++) {
      // Subtract out either the first (want change since the sequence start)
      // or the prior value (want change from one value to the next).
      std::size_t base = 0;

      if (gOutputDeltas && (j > 0)) base = j - 1;

      float baseVal = seq.m_values[base];

      if (gOutputAbsolute && !gOutputDeltas) baseVal = 0.0f;

      printf("\t%.2f", (seq.m_values[j] - baseVal));
    }
    printf("\n");
  }

  return 0;
}
