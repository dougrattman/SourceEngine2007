// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "utils/vmpi/imysqlwrapper.h"

#include <cstdio>

#include "base/include/windows/windows_light.h"

#include <winsock2.h>

extern "C" {
#include "deps/mysql/include/mysql.h"
};

static char *CopyString(const char *string_to_copy) {
  if (!string_to_copy) string_to_copy = "";

  std::size_t size{strlen(string_to_copy) + 1};
  char *result = new char[size];
  strcpy_s(result, size, string_to_copy);

  return result;
}

struct CopiedRow {
  ~CopiedRow() { m_Columns.PurgeAndDeleteElements(); }

  CUtlVector<char *> m_Columns;
};

class MySQLCopiedRowSet : public IMySQLRowSet {
 public:
  MySQLCopiedRowSet()
      : current_row_index_{-1}, copied_rows_{}, column_names_{} {}

  ~MySQLCopiedRowSet() {
    copied_rows_.PurgeAndDeleteElements();
    column_names_.PurgeAndDeleteElements();
  }

  void Release() override { delete this; }

  int NumFields() override { return column_names_.Count(); }

  const char *GetFieldName(int iColumn) override {
    return column_names_[iColumn];
  }

  bool NextRow() override {
    ++current_row_index_;
    return current_row_index_ < copied_rows_.Count();
  }

  bool SeekToFirstRow() override {
    current_row_index_ = 0;
    return current_row_index_ < copied_rows_.Count();
  }

  CColumnValue GetColumnValue(int iColumn) override {
    return CColumnValue{this, iColumn};
  }

  CColumnValue GetColumnValue(const char *pColumnName) override {
    return CColumnValue{this, GetColumnIndex(pColumnName)};
  }

  const char *GetColumnValue_String(int iColumn) override {
    if (iColumn < 0 || iColumn >= column_names_.Count())
      return "<invalid column specified>";

    if (current_row_index_ < 0 || current_row_index_ >= copied_rows_.Count())
      return "<invalid row specified>";

    return copied_rows_[current_row_index_]->m_Columns[iColumn];
  }

  long GetColumnValue_Int(int iColumn) override {
    return atoi(GetColumnValue_String(iColumn));
  }

  int GetColumnIndex(const char *pColumnName) override {
    for (int i = 0; i < column_names_.Count(); i++) {
      if (_stricmp(column_names_[i], pColumnName) == 0) return i;
    }

    return -1;
  }

 public:
  int current_row_index_;
  CUtlVector<CopiedRow *> copied_rows_;
  CUtlVector<char *> column_names_;
};

class MySQL : public IMySQL {
 public:
  MySQL() : mysql_{nullptr}, mysql_result_{nullptr}, mysql_row_{nullptr} {}

  virtual ~MySQL() {
    CancelIteration();

    if (mysql_) {
      mysql_close(mysql_);
      mysql_ = nullptr;
    }
  }

  bool InitMySQL(const char *db_name, const char *host_name,
                 const char *user_name, const char *password) override {
    MYSQL *mysql = mysql_init(nullptr);
    if (!mysql) return false;

    if (!mysql_real_connect(mysql, host_name, user_name, password, db_name, 0,
                            nullptr, 0)) {
      strcpy_s(mysql_last_error_, mysql_error(mysql));
      mysql_close(mysql);

      return false;
    }

    mysql_ = mysql;
    return true;
  }

  void Release() override { delete this; }

  int Execute(const char *pString) override {
    CancelIteration();

    int result{mysql_query(mysql_, pString)};
    if (result == 0) {
      // Is this a query with a result set?
      mysql_result_ = mysql_store_result(mysql_);

      if (mysql_result_) {
        // Store the field information.
        unsigned int count{mysql_num_fields(mysql_result_)};
        MYSQL_FIELD *fields{mysql_fetch_fields(mysql_result_)};

        mysql_fields_.CopyArray(fields, count);

        return 0;
      }

      // No result set. Was a set expected?
      if (mysql_field_count(mysql_) != 0) return 1;
    }

    return result;
  }

  IMySQLRowSet *DuplicateRowSet() override {
    MySQLCopiedRowSet *row_set = new MySQLCopiedRowSet;
    row_set->current_row_index_ = -1;
    row_set->column_names_.SetSize(mysql_fields_.Count());

    for (int i = 0; i < mysql_fields_.Count(); i++)
      row_set->column_names_[i] = CopyString(mysql_fields_[i].name);

    while (NextRow()) {
      CopiedRow *row = new CopiedRow;
      row_set->copied_rows_.AddToTail(row);
      row->m_Columns.SetSize(mysql_fields_.Count());

      for (int i = 0; i < mysql_fields_.Count(); i++) {
        row->m_Columns[i] = CopyString(mysql_row_[i]);
      }
    }

    return row_set;
  }

  u64 InsertID() override { return mysql_insert_id(mysql_); }

  int NumFields() override { return mysql_fields_.Count(); }

  const char *GetFieldName(int iColumn) override {
    return mysql_fields_[iColumn].name;
  }

  bool NextRow() override {
    if (!mysql_result_) return false;

    mysql_row_ = mysql_fetch_row(mysql_result_);

    return mysql_row_ != nullptr;
  }

  bool SeekToFirstRow() override {
    if (!mysql_result_) return false;

    mysql_data_seek(mysql_result_, 0);

    return true;
  }

  CColumnValue GetColumnValue(int iColumn) override {
    return CColumnValue(this, iColumn);
  }

  CColumnValue GetColumnValue(const char *pColumnName) override {
    return CColumnValue(this, GetColumnIndex(pColumnName));
  }

  const char *GetColumnValue_String(int iColumn) override {
    if (mysql_row_ && iColumn >= 0 && iColumn < mysql_fields_.Count() &&
        mysql_row_[iColumn])
      return mysql_row_[iColumn];

    return "";
  }

  long GetColumnValue_Int(int iColumn) override {
    return atol(GetColumnValue_String(iColumn));
  }

  int GetColumnIndex(const char *pColumnName) override {
    for (int i = 0; i < mysql_fields_.Count(); i++) {
      if (_stricmp(pColumnName, mysql_fields_[i].name) == 0) {
        return i;
      }
    }

    return -1;
  }

  const char *GetLastError() override {
    // Default to the last error if mysql_ was not successfully initialized
    return mysql_ ? mysql_error(mysql_) : mysql_last_error_;
  }

  // Cancels the storage of the rows from the latest query.
  void CancelIteration() {
    mysql_fields_.Purge();

    if (mysql_result_) {
      mysql_free_result(mysql_result_);
      mysql_result_ = nullptr;
    }

    mysql_row_ = nullptr;
  }

 private:
  MYSQL *mysql_;
  MYSQL_RES *mysql_result_;
  MYSQL_ROW mysql_row_;
  CUtlVector<MYSQL_FIELD> mysql_fields_;

  char mysql_last_error_[128];
};

EXPOSE_INTERFACE(MySQL, IMySQL, MYSQL_WRAPPER_VERSION_NAME);
