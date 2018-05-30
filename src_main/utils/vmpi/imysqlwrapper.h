// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MYSQL_WRAPPER_H
#define MYSQL_WRAPPER_H

#include "tier1/interface.h"
#include "tier1/utlvector.h"

#include "base/include/base_types.h"
#include "base/include/compiler_specific.h"

class CColumnValue;

the_interface IMySQLRowSet {
 public:
  virtual void Release() = 0;

  // Get the number of columns in the data returned from the last query (if it
  // was a select statement).
  virtual int NumFields() = 0;

  // Get the name of each column returned by the last query.
  virtual const char *GetFieldName(int iColumn) = 0;

  // Call this in a loop until it returns false to iterate over all rows the
  // query returned.
  virtual bool NextRow() = 0;

  // You can call this to start iterating over the result set from the start
  // again. Note: after calling this, you have to call NextRow() to actually get
  // the first row's value ready.
  virtual bool SeekToFirstRow() = 0;

  virtual CColumnValue GetColumnValue(int iColumn) = 0;
  virtual CColumnValue GetColumnValue(const char *pColumnName) = 0;

  virtual const char *GetColumnValue_String(int iColumn) = 0;
  virtual long GetColumnValue_Int(int iColumn) = 0;

  // You can call this to get the index of a column for faster lookups with
  // GetColumnValue( int ). Returns -1 if the column can't be found.
  virtual int GetColumnIndex(const char *pColumnName) = 0;
};

class CColumnValue {
 public:
  CColumnValue(IMySQLRowSet *mysql_row_set, int column)
      : mysql_row_set_{mysql_row_set}, column_{column} {}

  const char *String() {
    return mysql_row_set_->GetColumnValue_String(column_);
  }
  long Int32() { return mysql_row_set_->GetColumnValue_Int(column_); }

 private:
  IMySQLRowSet *mysql_row_set_;
  const int column_;
};

the_interface IMySQL : public IMySQLRowSet {
 public:
  virtual bool InitMySQL(const char *pDBName, const char *pHostName = "",
                         const char *pUserName = "",
                         const char *pPassword = "") = 0;
  virtual void Release() = 0;

  // These execute SQL commands. They return 0 if the query was successful.
  virtual int Execute(const char *pString) = 0;

  // This reads in all of the data in the last row set you queried with Execute
  // and builds a separate copy. This is useful in some of the VMPI tools to
  // have a thread repeatedly execute a slow query, then store off the results
  // for the main thread to parse.
  virtual IMySQLRowSet *DuplicateRowSet() = 0;

  // If you just inserted rows into a table with an AUTO_INCREMENT column,
  // then this returns the (unique) value of that column.
  virtual u64 InsertID() = 0;

  // Returns the last error message, if an error took place
  virtual const char *GetLastError() = 0;
};

#define MYSQL_WRAPPER_VERSION_NAME "MySQLWrapper001"

#endif  // MYSQL_WRAPPER_H
