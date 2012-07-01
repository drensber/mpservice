/* 
 * libmpsserver-library.c
 *   This contains the methods of the libmpsserver library that can be 
 *   used to navigate the hierarchically organized media database.
 *
 */

#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include "libmpsserver-private.h"

// MP3_ONLY is temporary until multiple file formats are supported
//
#define MP3_ONLY

static int database_element_list_get(mps_library_element_t **element_list,
				     char *query1, char *query2, char *query3);
static char *get_file_path_from_id(long id, char *path, sqlite3 *db, int *rc);
static int compare_elements_ignore_a_an_the(const void *p1, const void *p2);
static int mps_library_get_one_int64_value(char *query, long *value);
static inline int mps_library_db_open(sqlite3 **db);

/* Here's where the real function definitions start */
int mps_library_hierarchy_get_tag_root(mps_library_folder_t **folder)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int)folder);

  mps_library_hierarchy_get(folder, MPS_ARTIST_ID_ROOT);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_library_hierarchy_get_file_path_root(mps_library_folder_t **folder)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int)folder);

  mps_library_hierarchy_get(folder, MPS_FILE_PATH_ID_ROOT);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_library_hierarchy_get(mps_library_folder_t **folder, long id)
{
  int rv=MPS_SUCCESS;
  int sqlite_rc, num;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  char query_string[MAX_SQL_STATEMENT_LENGTH], 
    query_string_two[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s(0x%08x, %ld)\n", 
		 __func__, (unsigned int)folder, id);

  if ((rv = mps_library_db_open(&db))
      != MPS_SUCCESS) {
    goto OUT;
  }

  *folder = malloc(sizeof(mps_library_folder_t));
  (*folder)->id = id;

  if (id == MPS_FILE_PATH_ID_ROOT) {
    (*folder)->parent = MPS_FILE_PATH_ID_ROOT;
    strncpy((*folder)->name, "(file path root)",
	    MPS_MAX_FOLDERNAME_LENGTH);
  }
  else if (id == MPS_ARTIST_ID_ROOT) {
    (*folder)->parent = MPS_ARTIST_ID_ROOT;
    strncpy((*folder)->name, "(artist root)",
	    MPS_MAX_FOLDERNAME_LENGTH);
  }
  else {
    if (MPS_IS_FILE_PATH_ID(id)) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	       "select parent_id,name from mps_library_file_path "
	       "where id=%ld;",
	       id);
    }
    else if (MPS_IS_ARTIST_ID(id)) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	       "select %ld,artist from mps_library_artists "
	       "where artist_id=%ld;",
	       MPS_ARTIST_ID_ROOT, id);
    }
    else if (MPS_IS_ALBUM_ID(id)) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	       "select artist_id,album from mps_library_albums "
	       "where album_id=%ld;",
	       id);
    }
    else {
      MPS_LOG("ERROR: mps_library_folder_get() called with invalid id:%ld\n", id);
      rv = MPS_FAILURE;
      sqlite3_close(db);
      goto OUT;
    }

    MPS_DBG_PRINT("perparing statement:\n%s\n", query_string);
    sqlite_rc = sqlite3_prepare_v2(db, query_string, -1, &stmt, 0);
    if(sqlite_rc != SQLITE_OK ) { 
      MPS_LOG("SQL error: %d\n", sqlite_rc);
      rv = MPS_FAILURE;
      sqlite3_close(db);
      goto OUT;
    }
    
    if(sqlite3_step(stmt) == SQLITE_ROW) {
      (*folder)->parent = sqlite3_column_int64(stmt, 0);
      strncpy((*folder)->name, (char *)sqlite3_column_text(stmt, 1),
	      MPS_MAX_FOLDERNAME_LENGTH); 
      sqlite3_finalize(stmt);
    }
    else {
      MPS_LOG("Couldn't info for id=%ld\n", id);
      rv = MPS_FAILURE;
      sqlite3_close(db);
      goto OUT;
    }
    sqlite3_close(db);
  }
  
  if (MPS_IS_FILE_PATH_ID(id)) {
    snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	     "select id,name from mps_library_file_path "
             "where parent_id=%ld "
	     "order by name; ",
	     id);
    snprintf(query_string_two, MAX_SQL_STATEMENT_LENGTH,
	     "select title_id,file_name from mps_library_titles "
#ifdef MP3_ONLY
             "where parent_path_id=%ld and file_type=1 "
#else
             "where parent_path_id=%ld "
#endif
	     "order by file_name; ",
	     id);
  }
  else if (MPS_IS_ARTIST_ID(id)) {
    if (id == MPS_ARTIST_ID_ROOT) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	       "select artist_id,artist from mps_library_artists; ");    
    }
    else {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	       "select album_id,album from mps_library_albums "
               "where artist_id=%ld "
	       "order by album; ",
	       id);          
    }
    query_string_two[0] = '\0';
  }
  else if (MPS_IS_ALBUM_ID(id)) {
    /* ordering by file_name is sort of a hack because the 
       track order is not available from mpg123. After switch
       to TagLib, this should be changed. */
    snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	     "select title_id,title from mps_library_titles "
#ifdef MP3_ONLY
             "where album_id=%ld and file_type=1 "
#else
             "where album_id=%ld "
#endif
             "order by ordinal_track_number; ",
	     id);          
    query_string_two[0] = '\0';
  }

  if ((num = database_element_list_get(&((*folder)->elements), 
				       query_string, query_string_two, NULL))
      != MPS_FAILURE) {
    (*folder)->number_of_elements = num;

    if (MPS_IS_ARTIST_ID(id)) {
      // use qsort() to sort (*folder)->elements alphebetically,
      // but ignore "the ", "a ", "an ";
      qsort((*folder)->elements, (*folder)->number_of_elements, 
            sizeof(mps_library_element_t), compare_elements_ignore_a_an_the);      
    }
    rv = MPS_SUCCESS;
  }
  else {
    rv = MPS_FAILURE;
  }

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_library_folder_free(mps_library_folder_t *folder)
{
  int rv=MPS_SUCCESS;
  int i;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int)folder);

  if (folder->number_of_elements > 0) {
    for (i = 0; i < folder->number_of_elements; i++) {
      free(folder->elements[i].name);
    }
    free(folder->elements);
  }
  free(folder);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


int mps_library_title_get_info(int track_id, mps_library_track_info_t *info)
{
  int rv=MPS_SUCCESS;
  int sqlite_rc;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  char query_string[MAX_SQL_STATEMENT_LENGTH];

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int)info);

  if ((rv = mps_library_db_open(&db))
      != MPS_SUCCESS) {
    goto OUT;
  }

  snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	   "select * from mps_library_titles where title_id=%d;",
	   track_id); 

  sqlite_rc = sqlite3_prepare_v2(db, query_string, -1, &stmt, 0);
  if( sqlite_rc ){
    MPS_LOG("Sqlite3 error (%s)\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return MPS_FAILURE;
  }
  
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    info->title_id = sqlite3_column_int64(stmt, 0);
    info->file_length = sqlite3_column_int64(stmt, 2);
    info->file_last_modification = sqlite3_column_int64(stmt, 3);
    info->parent_path_id = sqlite3_column_int64(stmt, 4);
    strncpy(info->file_name, (char *)sqlite3_column_text(stmt, 5), 
	    MPS_MAX_LIBRARY_PATH_LENGTH);
    info->file_type = sqlite3_column_int64(stmt, 6);
    strncpy(info->title, (char *)sqlite3_column_text(stmt, 7), 
	    MPS_MAX_TRACK_INFO_FIELD_LENGTH);
    info->album_id = sqlite3_column_int64(stmt, 8);
    info->artist_id = sqlite3_column_int64(stmt, 9);
    info->year = sqlite3_column_int(stmt, 10);
    strncpy(info->genre, (char *)sqlite3_column_text(stmt, 11), 
	    MPS_MAX_TRACK_INFO_FIELD_LENGTH);
    info->time_length = sqlite3_column_int(stmt, 12);
    info->play_count = sqlite3_column_int(stmt, 13);
    info->last_played = sqlite3_column_int64(stmt, 14);
    info->ordinal_track_number = sqlite3_column_int(stmt, 15);
    info->bitrate = sqlite3_column_int(stmt, 16);
    info->sampling_rate = sqlite3_column_int(stmt, 17);
    info->channel_mode = sqlite3_column_int(stmt, 18);
    info->vbr_mode = sqlite3_column_int(stmt, 19);

    sqlite3_finalize(stmt);

    snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	     "select album from mps_library_albums where album_id=%ld;",
	     info->album_id); 
    sqlite_rc = sqlite3_prepare_v2(db, query_string, -1, &stmt, 0);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      strncpy(info->album, (char *)sqlite3_column_text(stmt, 0), 
	      MPS_MAX_TRACK_INFO_FIELD_LENGTH);
      sqlite3_finalize(stmt);
    }
    else {
      MPS_LOG("Couldn't get track info for album_id=%ld\n", info->album_id);
      rv = MPS_FAILURE;
    }

    snprintf(query_string, MAX_SQL_STATEMENT_LENGTH, 
	     "select artist from mps_library_artists where artist_id=%ld;",
	     info->artist_id); 
    sqlite_rc = sqlite3_prepare_v2(db, query_string, -1, &stmt, 0);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      strncpy(info->artist, (char *)sqlite3_column_text(stmt, 0), 
	      MPS_MAX_TRACK_INFO_FIELD_LENGTH);
      sqlite3_finalize(stmt);
    }
    else {
      MPS_LOG("Couldn't get track info for artist_id=%ld\n", info->artist_id);
      rv = MPS_FAILURE;
    }
  }
  else {
    MPS_LOG("Couldn't get track info for track_id=%d\n", track_id);
    rv = MPS_FAILURE;
  }


  sqlite3_close(db);
  
 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_library_get_file_path_from_id(long id, char *path) {
  int rv = MPS_SUCCESS;
  sqlite3 *db;

  MPS_DBG_PRINT("Entering %s(%ld, 0x%08x(\"%s\"))\n", __func__, id, 
		 (unsigned int)path, path);

  if ((rv = mps_library_db_open(&db))
      != MPS_SUCCESS) {
    goto OUT;
  }

  get_file_path_from_id(id, path, db, &rv);

  sqlite3_close(db);
  
 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static char *get_file_path_from_id(long id, char *path, sqlite3 *db, int *rv)
{
  sqlite3_stmt *stmt = NULL;
  int sqlite_rc;
  char query_string[MAX_SQL_STATEMENT_LENGTH];

  long parent_id; 
  char name[MPS_MAX_FOLDERNAME_LENGTH];
  mps_config_param_t config_param;

  MPS_DBG_PRINT("Entering %s(%ld, 0x%08x(\"%s\", 0x%08x))\n", __func__, id, 
		 (unsigned int)path, path, (unsigned int)db);
  
  if (! (MPS_IS_TITLE_ID(id) || MPS_IS_FILE_PATH_ID(id)) ) {
    MPS_LOG("%ld is not a track id or file path id\n", id); 
    *rv = MPS_FAILURE;
    return path;
  }

  if (id == MPS_FILE_PATH_ID_ROOT) {
    // Note: when I get around to implementing the "virtual root" (rather
    // than just using media_dir__0 as the root), the 
    // if (id == MPS_FILE_PATH_ID_ROOT) case will just return ""
    strcpy(config_param.name, "media_dir__0");
    *rv = mps_configuration_get_param(&config_param);
    if (*rv != MPS_SUCCESS) {
      MPS_LOG("Couldn't find value in config file for \"media_dir__0\"");
    }
    else {
      snprintf(path, MPS_MAX_FOLDERNAME_LENGTH,
	       "%s", config_param.value);
    }

    return path;
  }
  else {
    if (MPS_IS_TITLE_ID(id)) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH,
	       "select parent_path_id,file_name from mps_library_titles "
	       "where title_id=%ld; ",
	       id);
    }
    else if (MPS_IS_FILE_PATH_ID(id)) {
      snprintf(query_string, MAX_SQL_STATEMENT_LENGTH,
	       "select parent_id,name from mps_library_file_path where id=%ld; ",
	       id);
    }

    MPS_DBG_PRINT("perparing statement:\n%s\n", query_string);
    sqlite_rc = sqlite3_prepare_v2(db, query_string, -1, &stmt, 0);
    if(sqlite_rc != SQLITE_OK ) { 
      MPS_LOG("SQL error: %d\n", sqlite_rc);
      *rv = MPS_FAILURE;
      return path;
    }
    if(sqlite3_step(stmt) == SQLITE_ROW) {
      parent_id = sqlite3_column_int64(stmt, 0);
      snprintf(name, MPS_MAX_FOLDERNAME_LENGTH,
	       "/%s", (char *)sqlite3_column_text(stmt, 1));
      sqlite3_finalize(stmt);
    }    

    return strcat(get_file_path_from_id(parent_id, path, db, rv), name);
  }
}

long mps_library_get_artist_count()
{
  long rv;

  if (mps_library_get_one_int64_value
      ("select count(*) from mps_library_artists; ", &rv)
      != MPS_SUCCESS) {
    rv = MPS_FAILURE;
  }
                                 
  return rv;
}

long mps_library_get_album_count()
{
  long rv;

  if (mps_library_get_one_int64_value
      ("select count(*) from mps_library_albums; ", &rv)
      != MPS_SUCCESS) {
    rv = MPS_FAILURE;
  }

  return rv;
}

long mps_library_get_title_count()
{
  long rv;

  if (mps_library_get_one_int64_value
      ("select count(*) from mps_library_titles; ", &rv)
      != MPS_SUCCESS) {
    rv = MPS_FAILURE;
  }

  return rv;
}

int mps_library_artist_list_get(mps_library_folder_t **list)
{
  return mps_library_hierarchy_get(list, MPS_ARTIST_ID_ROOT);
}

int mps_library_album_list_get(mps_library_folder_t **list, int artist_id)
{
  return mps_library_hierarchy_get(list, artist_id);
}

int mps_library_title_list_get(mps_library_folder_t **list, int album_id)
{
  return mps_library_hierarchy_get(list, album_id);
}

int mps_library_random(mps_library_folder_t **list, 
                       mps_database_object_type_t object_type, 
                       int count)
{
  int rv=MPS_SUCCESS;
  char query[MAX_SQL_STATEMENT_LENGTH];
  int num;

  MPS_DBG_PRINT("Entering %s(0x%08x, %d)\n", __func__, 
		 (unsigned int)list, count);

  *list = malloc(sizeof(mps_library_folder_t));

  switch (object_type) {
  case MPS_OBJECT_TYPE_TITLE:
    sprintf(query, 
            "select title_id, title, file_type from mps_library_titles "
#ifdef MP3_ONLY
            "where file_type=1 order by random() limit %d", count);
#else
            "order by random() limit %d", count);
#endif
    break;
  case MPS_OBJECT_TYPE_ARTIST:
    sprintf(query, 
            "select artist_id, artist from mps_library_artists "
            "order by random() limit %d", count);
    break;
  case MPS_OBJECT_TYPE_ALBUM:
    sprintf(query, 
            "select album_id, album from mps_library_albums "
            "order by random() limit %d", count);
    break;
  case MPS_OBJECT_TYPE_FILE_PATH:
    sprintf(query, 
            "select id, name from mps_library_file_path "
            "order by random() limit %d", count);
    break;
  default:
    rv = MPS_FAILURE;
    goto OUT;
    break;
  }
  if ((num = database_element_list_get(&((*list)->elements), query, 
                                       NULL, NULL))
      != MPS_FAILURE) {
    (*list)->number_of_elements = num;
    (*list)->parent = MPS_NULL_OBJECT_ID;
    strcpy((*list)->name, " - random - ");
    (*list)->id = MPS_NULL_OBJECT_ID;

    rv = MPS_SUCCESS;
  }
  else {
    rv = MPS_FAILURE;
  }

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;  
}

/* "limit" is the limit per sql search */
int mps_library_simple_search(mps_library_folder_t **list, char *search_string,
                              mps_database_object_type_t object_type, int limit)
{ 
  int rv=MPS_SUCCESS;
  char query[MAX_SQL_STATEMENT_LENGTH];
  int num;

  MPS_DBG_PRINT("Entering %s(0x%08x, \"%s\")\n", 
		 __func__, (unsigned int)list, search_string);

  *list = malloc(sizeof(mps_library_folder_t));


  if (search_string[0] == '\0') {
    (*list)->number_of_elements = 0;
    (*list)->parent = MPS_NULL_OBJECT_ID;
    sprintf((*list)->name, " - search for \"\" - ");
    (*list)->id = MPS_NULL_OBJECT_ID;
    (*list)->elements = NULL;
    rv = MPS_SUCCESS;
    goto OUT;
  }

  switch (object_type) {
  case MPS_OBJECT_TYPE_TITLE:
    sprintf(query, 
            "select title_id, title, file_type from mps_library_titles "
#ifdef MP3_ONLY
            "where title like '%%%s%%' and file_type=1 limit %d; ", search_string, limit);
#else
            "where title like '%%%s%%' limit %d; ", search_string, limit);
#endif
    break;
  case MPS_OBJECT_TYPE_ARTIST:
    sprintf(query, 
            "select artist_id, artist from mps_library_artists "
            "where artist like '%%%s%%' limit %d; ", search_string, limit);
    break;
  case MPS_OBJECT_TYPE_ALBUM:
    sprintf(query, 
            "select album_id, album from mps_library_albums "
            "where album like '%%%s%%' limit %d; ", search_string, limit);
    break;
  case MPS_OBJECT_TYPE_FILE_PATH:
    sprintf(query, 
            "select id, name from mps_library_file_path "
            "where name like '%%%s%%' limit %d; ", search_string, limit);
    break;
  default:
    rv = MPS_FAILURE;
    goto OUT;
    break;
  }

  if ((num = database_element_list_get(&((*list)->elements), 
                                       query, NULL, NULL))
      != MPS_FAILURE) {
    (*list)->number_of_elements = num;
    (*list)->parent = MPS_NULL_OBJECT_ID;
    sprintf((*list)->name, " - search for \"%s\" - ", search_string);
    (*list)->id = MPS_NULL_OBJECT_ID;

    rv = MPS_SUCCESS;
  }
  else {
    rv = MPS_FAILURE;
  }

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

//unimplemented

int mps_library_album_list_random(mps_library_folder_t **list, int count)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x, %d)\n", __func__, 
		 (unsigned int)list, count);

  MPS_LOG("%s unimplemented.\n", __func__);
  rv=MPS_FAILURE;

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;  
}

int mps_library_artist_list_random(mps_library_folder_t **list, int count)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x, %d)\n", __func__, 
		 (unsigned int)list, count);

  MPS_LOG("%s unimplemented.\n", __func__);
  rv=MPS_FAILURE;

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;  
}


extern int mps_library_sql_search(mps_library_folder_t **list, 
                                  char *where_clause)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x, \"%s\"\n", 
		 __func__, (unsigned int)list, where_clause);

  MPS_LOG("%s unimplemented.\n", __func__);
  rv=MPS_FAILURE;

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_library_get_status(mps_library_status_t *status)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int) status);
  memcpy(status, &mps_shm_region->library_status, 
	 sizeof(mps_library_status_t));
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", 
		 __func__, rv);
  return rv;
}

/* database_element_list_get() -
   Assumes a query (or queries) that returns 1-3 columns 
   (in name [, id] [, file_type] order) of data and places
   it in a database_string_list_t.   
   
   If sql_query_two is not NULL, it's results will be added to the list too
   (maybe should convert to a va_args, type interface in the future?).
 
   returns - number of elements retrieved, or MPS_FAILURE on error.
*/
static int database_element_list_get(mps_library_element_t **element_list,
				     char *query1, char *query2, char *query3)
{
  int i, sqlite_rc, query_result_count;
  mps_library_element_t *list_ptr;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  mps_library_element_t tmp_element_list[MPS_MAX_ELEMENTS_PER_LIST];
  char argc, *argv[3];

  MPS_DBG_PRINT("Entering %s(0x%08x, \"%s\", \"%s\")\n", __func__, 
		(unsigned int)element_list, query1, 
		(query2==NULL?"":query2));

  // should I just use varargs?   Where's my K&R?
  argc = ((query2==NULL||query2[0]=='\0')?1:
          (query3==NULL||query3[0]=='\0')?2:
          3);
  argv[0] = query1;
  argv[1] = query2;
  argv[2] = query3;

  if (mps_library_db_open(&db) != MPS_SUCCESS) {
    return MPS_FAILURE;
  }

  for (query_result_count = 0, i = 0; i < argc; i++) {

    sqlite_rc = sqlite3_prepare_v2(db, argv[i], -1, &stmt, 0);
    if(sqlite_rc != SQLITE_OK ) { 
      MPS_LOG("SQL error: %d\n", sqlite_rc);
      return MPS_FAILURE;
    }

    for( ; sqlite3_step(stmt) == SQLITE_ROW; query_result_count++) {
      
      tmp_element_list[query_result_count].name 
	= malloc(MPS_MAX_TRACK_INFO_FIELD_LENGTH);

      tmp_element_list[query_result_count].id 
	= (int) sqlite3_column_int64(stmt, 0);

      strncpy(tmp_element_list[query_result_count].name,
	      (char *) sqlite3_column_text(stmt, 1),
	      MPS_MAX_TRACK_INFO_FIELD_LENGTH);
    }
  }

  *element_list = 
    malloc(query_result_count * sizeof(mps_library_element_t));

  list_ptr = *element_list;

  for (i = 0; i < query_result_count; i++) {
    list_ptr[i].name = tmp_element_list[i].name;
    list_ptr[i].id = tmp_element_list[i].id;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, i);
  return query_result_count;
}

static inline int mps_library_db_open(sqlite3 **db)
{
  int sqlite_rc, rv=MPS_SUCCESS;
  static mps_config_param_t library_db_filename_param;

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned)db); 

  /* if the first character is the null character, assume that 
     library_db_filename_param value hasn't been initialized, and initialize 
     it from the mpservice_constants.xml file.  Otherwise do nothing. */
  if (library_db_filename_param.value[0] == 0) {
    strcpy(library_db_filename_param.name, "live_librarydb_file");
    rv = mps_constants_get_param(&library_db_filename_param);
    if (rv != MPS_SUCCESS) {
      library_db_filename_param.value[0] = 0;
      MPS_LOG("Can't get library_db_filename_param\n");
      goto OUT;
    }
  }

  sqlite_rc = sqlite3_open(library_db_filename_param.value, db);
  if( sqlite_rc ){
    MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(*db));
    rv = MPS_FAILURE;
    goto OUT;
  }
  sqlite3_busy_timeout(*db, MPS_COMMAND_TIMEOUT_SEC*1000);  

 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


static int compare_elements_ignore_a_an_the(const void *p1, const void *p2)
{
  char *string1 = ((mps_library_element_t *)p1)->name, 
    *string2 = ((mps_library_element_t *)p2)->name;
  
  if (string1[0] == 'T' || string1[0] == 't')
    if (string1[1] == 'H' || string1[1] == 'h')
      if (string1[2] == 'E' || string1[2] == 'e')
        if (string1[3] == ' ')
          string1 = &(string1[4]);

  if (string1[0] == 'A' || string1[0] == 'a') {
    if (string1[1] == ' ')
      string1 = &(string1[2]);
    else
      if (string1[1] == 'N' || string1[1] == 'n')
        if (string1[2] == ' ')
          string1 = &(string1[3]);
  }

  if (string2[0] == 'T' || string2[0] == 't')
    if (string2[1] == 'H' || string2[1] == 'h')
      if (string2[2] == 'E' || string2[2] == 'e')
        if (string2[3] == ' ')
          string2 = &(string2[4]);

  if (string2[0] == 'A' || string2[0] == 'a') {
    if (string2[1] == ' ')
      string2 = &(string2[2]);
    else
      if (string2[1] == 'N' || string2[1] == 'n')
        if (string2[2] == ' ')
          string2 = &(string2[3]);
  }

  return strcasecmp(string1, string2);
}

static int mps_library_get_one_int64_value(char *query, long *value)
{
  int rv;
  sqlite3 *db = NULL;

  if (mps_library_db_open(&db) == MPS_SUCCESS) {
    rv = get_one_int64_value(db, query, value);
  }
  else {
    rv = MPS_FAILURE;
    goto OUT;
  }

  rv = sqlite3_close(db);
  
 OUT:
  return rv;
}
