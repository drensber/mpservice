/* 
 * libmpsserver-playlist.c
 *   This contains the methods of the libmpsserver library that operate
 *   on playlists (adding, removing, or retrieving entries).
 *
 */
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "libmpsserver-private.h"

static int mps_playlist_open(sqlite3 **db);
static void mps_playlist_close(sqlite3 *db);
static int playlist_add_folder_after(long first_id, long folder_id);
static int mps_playlist_get_one_int64_value(char *query, long *value);
static int mps_playlist_get_two_int64_values(char *query, 
                                             long *value1, long *value2);

//globals
mps_shm_region_t *mps_shm_region;
long highest_entry_id;


static int mps_playlist_open(sqlite3 **db)
{
  int rv = MPS_SUCCESS;
  int fd;
  int sqlite_rc;
  char *zErrMsg = 0;
  char *tableCreationStatement = 
    "create table mps_playlist ("
    "playlist_entry_id int64, title_id int64, next_playlist_entry_id int64"
    ");";
  char tableInitializationStatement[MAX_SQL_STATEMENT_LENGTH];
  mps_config_param_t playlist_file_param;

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned)db);

  // Get the name of the playlist file from mpservice_constants.xml
  strcpy(playlist_file_param.name, "live_playlist_file");
  rv = mps_constants_get_param(&playlist_file_param);

  if (rv != MPS_SUCCESS) {
    goto OUT;
  }
  
  // Just using open() to determine if file exists... don't forget to close it!
  if ((fd = open(playlist_file_param.value, O_EXCL)) == -1) {
    close(fd);
    MPS_DBG_PRINT("File \"%s\" doesn't exist... "
		  "creating and initializing it's database\n", 
		  playlist_file_param.value);
    sqlite_rc = 
      sqlite3_open(playlist_file_param.value, db);
    if( sqlite_rc ){
      MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(*db));
      rv = MPS_FAILURE;
      goto OUT;
    }
    else {
      MPS_DBG_PRINT("sqlite_open() succesfully returned object *db=0x%08x\n",
		    (unsigned)*db);
    }

    sqlite3_busy_timeout(*db, MPS_COMMAND_TIMEOUT_SEC*1000);

    snprintf(tableInitializationStatement, MAX_SQL_STATEMENT_LENGTH,
	     "%s "
	     "insert into mps_playlist values(%ld, 0, %ld);",
	     tableCreationStatement,
	     (long)MPS_PLAYLIST_BEGINNING, (long)MPS_PLAYLIST_END);
    MPS_DBG_PRINT("Creating tables with statements:\n%s\n",
		   tableInitializationStatement);
    sqlite_rc = sqlite3_exec(*db, tableInitializationStatement, 
			     NULL, 0, &zErrMsg);
    if( sqlite_rc != SQLITE_OK ){
      MPS_LOG("creating table - SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      sqlite3_close(*db);
      rv = MPS_FAILURE;
      goto OUT;
    } 
  }
  else {
    close(fd);
    MPS_DBG_PRINT("File \"%s\" already exists...just opening\n",
		   playlist_file_param.value);
    sqlite_rc = sqlite3_open(playlist_file_param.value, db);
    if( sqlite_rc ){
      MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(*db));
      rv = MPS_FAILURE;
      goto OUT;
    }
    else {
      MPS_DBG_PRINT("sqlite_open() succesfully returned object *db=0x%08x\n",
		    (unsigned)*db);
    }

    sqlite3_busy_timeout(*db, MPS_COMMAND_TIMEOUT_SEC*1000);
  }

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static void mps_playlist_close(sqlite3 *db)
{
  int rv;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned)db);
  rv = sqlite3_close(db);
  MPS_DBG_PRINT("sqlite3_close() returned %d\n", rv);
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

int mps_playlist_add_after(long first_id, int num_entries, long *id_list)
{
  int i, sqlite_rc;
  int rv = MPS_SUCCESS;
  char *zErrMsg = 0;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];
  long subsequent_id;
  int playlist_count;
  sqlite3 *db;

  MPS_DBG_PRINT("Entering %s(%ld, %d, 0x%08x)\n", 
		 __func__, first_id, num_entries, (unsigned int)id_list);

  if ((rv = mps_playlist_open(&db))
      < MPS_SUCCESS) {
    rv = MPS_FAILURE;
    goto OUT;
  }

  if ((playlist_count = mps_playlist_get_count()) < 0) {
    rv = MPS_FAILURE;
    goto CLOSE_OUT;
  }
    
  if (playlist_count == 0) {
    highest_entry_id = 0;
  }
  else {
    if (mps_playlist_get_one_int64_value
	("select MAX(playlist_entry_id) from mps_playlist; ",
	 &highest_entry_id)
	< MPS_SUCCESS) {
      rv = MPS_FAILURE;
      goto CLOSE_OUT;
    }
  }

  /* Even though this is an "add after" function, inserting in
     the database linked list is naturally a "add before" type
     operation since the links are all forward, so we need to 
     calculate the subsequent playlist id and insert before. 
  */
  if (first_id == MPS_PLAYLIST_END) {
    subsequent_id = (long)MPS_PLAYLIST_END;
  }
  else {
    snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	     "select next_playlist_entry_id from mps_playlist "
	     "where playlist_entry_id=%ld;", first_id);
    if (mps_playlist_get_one_int64_value(sqlQuery, &subsequent_id) < MPS_SUCCESS) {
      rv = MPS_FAILURE;
      goto CLOSE_OUT;
    }
  }

  for(i=0; i<num_entries; i++) {
    if (MPS_IS_FILE_PATH_ID (id_list[i]) ||
	MPS_IS_ALBUM_ID(id_list[i])) {
      playlist_add_folder_after((i == 0) ? first_id : highest_entry_id, 
				id_list[i]);
    }
    else if (MPS_IS_TITLE_ID(id_list[i])) {
      highest_entry_id++;
      snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	       "update mps_playlist set next_playlist_entry_id=%ld "
	       "where next_playlist_entry_id=%ld; " 
	       "insert into mps_playlist values(%ld, %ld, %ld); ",
	       highest_entry_id, 
	       subsequent_id,
	       highest_entry_id, id_list[i], subsequent_id);
      
      MPS_DBG_PRINT("Calling sqlite3_exec on statement:\n\"%s\"\n", 
		     sqlQuery);    
      sqlite_rc = sqlite3_exec(db, sqlQuery, NULL, 0, &zErrMsg);
      
      if( sqlite_rc != SQLITE_OK ){
	MPS_LOG("Adding to playlist SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
	rv = MPS_FAILURE;
	goto CLOSE_OUT;
      } 
    }
    else  {
      MPS_LOG("%s() : id %ld is not a title ID\n",
	      __func__, id_list[i]);
    }    
  }

  if (rv == MPS_SUCCESS) {
    mps_shm_region->playlist_status.change_counter++;
  }

 CLOSE_OUT:
  mps_playlist_close(db);

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


int mps_playlist_remove(long first_id, int count)
{
  int rv=MPS_SUCCESS;
  int i, sqlite_rc;
  long this_id, next_id;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];
  sqlite3 *db;
  char *zErrMsg = 0;

  MPS_DBG_PRINT("Entering %s(%ld)\n", __func__, first_id);

  // MPS_PLAYLIST_BEGINNING is a special case
  if (first_id == MPS_PLAYLIST_BEGINNING) {
    snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	     "select next_playlist_entry_id from mps_playlist "
	     "where playlist_entry_id=%ld",
	     (long)MPS_PLAYLIST_BEGINNING);
    if (mps_playlist_get_one_int64_value(sqlQuery, &first_id) < MPS_SUCCESS) {
      rv = MPS_FAILURE;
      goto OUT;
    }
  } 

  if ((rv = mps_playlist_open(&db))
      < MPS_SUCCESS) {
    rv = MPS_FAILURE;
    goto OUT;
  }

  for (i = 0, this_id = first_id;
       (i < count || count == MPS_PLAYLIST_ALL) && this_id != MPS_PLAYLIST_END;
       i++, this_id = next_id) {
    snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	     "select next_playlist_entry_id from mps_playlist "
	     "where playlist_entry_id=%ld;", this_id); 
    if (mps_playlist_get_one_int64_value(sqlQuery, &next_id) < MPS_SUCCESS) {
      rv = MPS_FAILURE;
      break;
    }

    snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	     "delete from mps_playlist where playlist_entry_id=%ld;",
	     this_id);

    MPS_DBG_PRINT("Calling sqlite3_exec on statement:\n\"%s\"\n", 
		   sqlQuery);    
    sqlite_rc = sqlite3_exec(db, sqlQuery, NULL, 0, &zErrMsg);
    
    if( sqlite_rc != SQLITE_OK ){
      MPS_LOG("Removing from playlist SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      rv = MPS_FAILURE;
      break;
    } 
  }

  snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	   "update mps_playlist set next_playlist_entry_id=%ld "
	   "where next_playlist_entry_id=%ld;",
	   this_id, first_id);
  
  MPS_DBG_PRINT("Calling sqlite3_exec on statement:\n\"%s\"\n", 
		 sqlQuery);    
  sqlite_rc = sqlite3_exec(db, sqlQuery, NULL, 0, &zErrMsg);
  
  if( sqlite_rc != SQLITE_OK ){
    MPS_LOG("Adding to playlist SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    rv = MPS_FAILURE;
  } 

  mps_shm_region->playlist_status.change_counter++;

  mps_playlist_close(db);
  
 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_playlist_get(mps_playlist_list_t **list, long first_id, int count)
{
  int rv=MPS_SUCCESS;
  int i;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];
  long this_id, title_id, next_id;
  mps_playlist_entry_t *playlist_entry_ptr;

  MPS_DBG_PRINT("Entering %s(0x%08x, %ld, %d)\n", __func__, 
		(unsigned int) *list, first_id, count);


  // MPS_PLAYLIST_END and MPS_PLAYLIST_EMPTY and count==0 are 
  // special cases (just return a zero-length mps_playlist_list_t).
  if (first_id == MPS_PLAYLIST_END ||
      first_id == MPS_PLAYLIST_EMPTY ||
      count == 0) {
    *list = malloc(sizeof(mps_playlist_list_t));
    (*list)->number_of_entries = 0;
    (*list)->entry_head = NULL;
  } 
  else {
    // MPS_PLAYLIST_BEGINNING is a special case
    if (first_id == MPS_PLAYLIST_BEGINNING) {
      snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	       "select next_playlist_entry_id from mps_playlist "
	       "where playlist_entry_id=%ld",
	       (long)MPS_PLAYLIST_BEGINNING);
      if (mps_playlist_get_one_int64_value(sqlQuery, &first_id) < MPS_SUCCESS) {
	rv = MPS_FAILURE;
	goto OUT;
      }
    } 

    *list = malloc(sizeof(mps_playlist_list_t));
    MPS_DBG_PRINT("mallocing *list at 0x%08x\n", (unsigned)*list);
    (*list)->entry_head = NULL;

    for(i = 0, this_id = first_id ; 
	(i < count || count == MPS_PLAYLIST_ALL) 
	  && this_id != MPS_PLAYLIST_END ; 
	i++, this_id = next_id) {
      snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	       "select title_id,next_playlist_entry_id from "
	       "mps_playlist where playlist_entry_id=%ld;  ",
	       this_id);
      if (mps_playlist_get_two_int64_values(sqlQuery, &title_id, &next_id)
	  < MPS_SUCCESS) {
	rv = MPS_FAILURE;
	goto OUT;
      }
       
      if (i == 0) {
	(*list)->entry_head = malloc(sizeof(mps_playlist_entry_t));
	playlist_entry_ptr = (*list)->entry_head; 
      }
      else {
	playlist_entry_ptr->next = malloc(sizeof(mps_playlist_entry_t));
	playlist_entry_ptr = playlist_entry_ptr->next;
      }
      playlist_entry_ptr->playlist_id = this_id;
      playlist_entry_ptr->title_id = title_id;
      playlist_entry_ptr->next = NULL;            
    }
    (*list)->number_of_entries = i;
  }  

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_playlist_free(mps_playlist_list_t **list)
{
  int rv=MPS_SUCCESS;
  mps_playlist_entry_t *this_entry_ptr, *next_entry_ptr;

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int) list);
    
  this_entry_ptr = (*list)->entry_head;
  while (this_entry_ptr != NULL) {    
    next_entry_ptr = this_entry_ptr->next;
    MPS_DBG_PRINT("freeing entry at 0x%08x\n", (unsigned)this_entry_ptr);
    free(this_entry_ptr);
    this_entry_ptr = next_entry_ptr;
  }

  MPS_DBG_PRINT("freeing *list at 0x%08x\n", (unsigned)*list);
  free(*list);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


long mps_playlist_get_next_id(long entry_id)
{
  long n, rv;
  long next_id;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s(%ld)\n", __func__, entry_id);

  if (entry_id == MPS_PLAYLIST_END) {
    rv = entry_id;
  }
  else {
    n = mps_playlist_get_count();

    if (n == 0) { /* the empty (NULL) case */
      rv = MPS_PLAYLIST_EMPTY;
    }
    else {
      // So that if the list was previously empty, but then had
      // entries added next_id(<previous>) returns the 1st entry.
      if (entry_id == MPS_PLAYLIST_EMPTY) {
	entry_id = MPS_PLAYLIST_BEGINNING;
      }

      snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	       "select next_playlist_entry_id from mps_playlist "
	       "where playlist_entry_id=%ld;",
	       entry_id);
      
      if (mps_playlist_get_one_int64_value(sqlQuery, &next_id) == MPS_FAILURE) {
	rv = MPS_FAILURE;
      }
      else {
	rv = next_id;
      }
    }
  }

  MPS_DBG_PRINT("Leaving %s(%ld) - return value %ld\n",
		   __func__, entry_id, rv);
  return rv;
}

long mps_playlist_get_previous_id(long entry_id)
{
  long rv;
  int n;
  long previous_id;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s(%ld)\n", __func__, entry_id);
  
  if (entry_id == MPS_PLAYLIST_BEGINNING) {
    rv = MPS_PLAYLIST_BEGINNING;
  }
  else {
    n = mps_playlist_get_count();
    
    if (n == 0) { /* the empty (NULL) case */
      rv = MPS_PLAYLIST_EMPTY;
    }
    else {
      if (entry_id == MPS_PLAYLIST_END) {
	rv = mps_playlist_get_last_id(entry_id);
      }
      else {
	snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
		 "select playlist_entry_id from mps_playlist "
		 "where next_playlist_entry_id=%ld;",
		 entry_id);

	if (mps_playlist_get_one_int64_value(sqlQuery, &previous_id) == MPS_FAILURE) {
	  rv = MPS_FAILURE;
	}
	else {
	  if (previous_id == MPS_PLAYLIST_BEGINNING) {
	    rv = entry_id;
	  }
	  else {
	    rv = previous_id;
	  }
	}
      }
    }
  }
  MPS_DBG_PRINT("Leaving %s(%ld) - return value %ld\n", 
	    __func__, entry_id, rv);

  return rv;
}

int mps_playlist_get_count()
{
  int rv = 0;
  long count;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];
  
  snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	   "select count(*) from mps_playlist where playlist_entry_id!=%ld;",
	   (long)MPS_PLAYLIST_BEGINNING);

  if (mps_playlist_get_one_int64_value(sqlQuery, &count) == MPS_FAILURE) {
    rv = MPS_FAILURE;
  }
  else {
    rv = (int)count;
  }
  
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", 
		__func__, rv);
  return rv;
}

long mps_playlist_get_last_id()
{
  int n;
  long last_id;
  long rv = MPS_PLAYLIST_EMPTY;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  n = mps_playlist_get_count();

  if (n == 0) {
    rv = MPS_PLAYLIST_EMPTY;
  }
  else {
    snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	     "select playlist_entry_id from mps_playlist "
	     "where next_playlist_entry_id=%d;",
	     MPS_PLAYLIST_END);

    if (mps_playlist_get_one_int64_value(sqlQuery, &last_id) == MPS_FAILURE) {
      rv = MPS_FAILURE;
    }
    else {
      rv = last_id;
    }
  }

  MPS_DBG_PRINT("Leaving %s() - return value %ld\n", 
		 __func__, rv);
  return rv;
}

long mps_playlist_get_title_id_from_entry_id(long entry_id)
{
  long rv, title_id;
  char sqlQuery[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s(%ld)\n", __func__, entry_id);

  snprintf(sqlQuery, MAX_SQL_STATEMENT_LENGTH,
	   "select title_id from mps_playlist "
	   "where playlist_entry_id=%ld;",
	   entry_id);

  if (mps_playlist_get_one_int64_value(sqlQuery, &title_id) == MPS_FAILURE) {
    rv = MPS_FAILURE;
  }
  else {
    rv = title_id;
  }
  
  MPS_DBG_PRINT("Leaving %s(%ld) - return value %ld\n", 
		__func__, entry_id, rv);
  return rv;
}


int mps_playlist_get_status(mps_playlist_status_t *status)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int) status);
  memcpy(status, &mps_shm_region->playlist_status, 
	 sizeof(mps_playlist_status_t));
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int playlist_add_folder_after(long first_id, long folder_id)
{
  int rv = MPS_SUCCESS;
  int i;
  mps_library_folder_t *folder;
  long *id_list;
  int id_list_count;

  MPS_DBG_PRINT("Entering %s(%ld, %ld\n", __func__, first_id, folder_id);
  
  if (! (MPS_IS_ALBUM_ID(folder_id) || MPS_IS_FILE_PATH_ID(folder_id)) ) {
    MPS_LOG("%s() : folder_id %ld is not an album ID or file path ID\n",
	    __func__, folder_id);
    rv = MPS_FAILURE;
    goto OUT;
  }

  rv = mps_library_hierarchy_get(&folder, folder_id);

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_hierarchy_get..\n");
  }
  else {
    id_list = malloc((folder->number_of_elements)*sizeof(long));

    for (i=0; i<folder->number_of_elements; i++) {
      id_list[i] = folder->elements[i].id;
      MPS_DBG_PRINT("id_list[%d] = %ld\n", i, id_list[i]);
    }
  }

  id_list_count = folder->number_of_elements;

  mps_library_folder_free(folder);
  mps_playlist_add_after(first_id, id_list_count , id_list);  
  
  free(id_list);

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int mps_playlist_get_one_int64_value(char *query, long *value)
{
  int rv;
  sqlite3 *db = NULL;
  rv = mps_playlist_open(&db);

  if (rv == MPS_SUCCESS) {
    rv = get_one_int64_value(db, query, value);
  }
  else {
    goto OUT;
  }
  
  mps_playlist_close(db);

 OUT:
  return rv;
}

static int mps_playlist_get_two_int64_values(char *query, 
                                             long *value1, long *value2)
{
  int rv;
  sqlite3 *db = NULL;
  rv = mps_playlist_open(&db);

  if (rv == MPS_SUCCESS) {
    rv = get_two_int64_values(db, query, value1, value2);
  }
  else {
    goto OUT;
  }
  
  mps_playlist_close(db);

 OUT:
  return rv;
}
