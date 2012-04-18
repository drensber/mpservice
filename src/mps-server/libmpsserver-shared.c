#include "libmpsserver-private.h"

int get_one_int64_value(sqlite3 *db, char *query, long *value)
{
  int rv;
  MPS_DBG_PRINT("Entering %s(%s, 0x%08x)\n", 
		__func__, query, (unsigned int)value);

  rv = get_two_int64_values(db, query, value, NULL);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", 
		 __func__, rv);
  return rv;
}

int get_two_int64_values(sqlite3 *db, char *query, long *value1, long *value2)
{
  int rv=MPS_SUCCESS;
  int sqlite_rc;
  sqlite3_stmt *stmt = NULL;
  MPS_DBG_PRINT("Entering %s(%s, 0x%08x, 0x%08x)\n", 
		__func__, query, (unsigned int)value1, (unsigned int)value2);


  sqlite_rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  if(sqlite_rc != SQLITE_OK ) { 
    MPS_LOG("SQL error: %d\n", sqlite_rc);
    rv = MPS_FAILURE;
  }
  else {
    sqlite_rc = sqlite3_step(stmt);
    
    if(sqlite_rc == SQLITE_ROW ) {
      *value1 = sqlite3_column_int64(stmt, 0);
      MPS_DBG_PRINT("*value1 = %ld\n", *value1);
      if(value2 != NULL) {
        *value2 = sqlite3_column_int64(stmt, 1);
        MPS_DBG_PRINT("*value2 = %ld\n", *value2);
      } 
    }
    else {
      MPS_LOG("Couldn't sqlite3_step() statement \"%s\"\n", query);
      rv = MPS_FAILURE;
    }
  }
  
  sqlite3_finalize(stmt);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", 
		__func__, rv);
  return rv;
}
