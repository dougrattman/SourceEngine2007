// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef POINT_BONUSMAPS_ACCESSOR_H
#define POINT_BONUSMAPS_ACCESSOR_H

void BonusMapChallengeUpdate(const char *pchFileName, const char *pchMapName,
                             const char *pchChallengeName, int iBest);
void BonusMapChallengeNames(char *pchFileName, size_t file_name_size,
                            char *pchMapName, size_t map_name_size,
                            char *pchChallengeName, size_t challenge_name_size);
void BonusMapChallengeObjectives(int &iBronze, int &iSilver, int &iGold);

#endif  // POINT_BONUSMAPS_ACCESSOR_H
