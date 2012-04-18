/*
 * mps-librarydbd.c - Implements the mps-librarydbd command.
 */

#include "mps-librarydbd.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define COPYFILE_OK             0
#define COPYFILE_READ_ERROR     1
#define COPYFILE_WRITE_ERROR    2
#define COPYFILE_OUTCLOSE_ERROR 4
#define COPYFILE_OUTOPEN_ERROR  8
#define COPYFILE_INCLOSE_ERROR  16
#define COPYFILE_INOPEN_ERROR   32 

// For testing (and timing) purposes, it's sometimes convenient
// to have this just run one iteration at a time.  For normal use
// NO_LOOP_FOR_TESTING should be undefined
#undef NO_LOOP_FOR_TESTING

typedef struct id_ll_node_t {
  long id;
  struct id_ll_node_t *next;
} id_ll_node_t;

int opt;
char **mps_librarydbd_tokens;
int mps_librarydbd_num_tokens;
long highestTitleId, highestAlbumId, highestArtistId, highestFilePathId;
mps_librarydbd_state_t *state;
long dbVersion, numberTitlesAdded, numberTitlesDeleted,
  numberArtistsAdded, numberArtistsDeleted, numberAlbumsAdded,
  numberAlbumsDeleted, numberFilePathsAdded, numberFilePathsDeleted;

// Declare the options struct with default values
// and set up a global pointer
mps_librarydbd_options_t mps_librarydbd_options =
  {
    0,     // debug_enabled
    DEBUG_TO_LOG,
    "",
    "",
    ""
  }; 
mps_librarydbd_options_t *mps_librarydbd_options_p = &mps_librarydbd_options;

static void usage(char *);
static int remove_stale_entries(sqlite3 *db);
static int place_ids_in_list(sqlite3 *db, char *query, id_ll_node_t **list);
static int free_id_list(id_ll_node_t *list);
static int get_max_id(sqlite3 *, char *, char *, long *);
static int wait_for_changes_to_settle(char *, long *);
static int copy_unix_file(const char *, const char *); 

#ifndef UNIT_TEST
int main(int argc, char *argv[])
{
  mps_config_param_t shmfile_param;
  int rv=MPS_LIBRARYDBD_SUCCESS;

  while ((opt=getopt(argc, argv, "d:f:p:m:")) != -1) {
    switch(opt) {
    case 'd':
      mps_librarydbd_options_p->debug_enabled = 1;
      mps_librarydbd_options_p->debug_sink = 
	((optarg[0] == 'c')?DEBUG_TO_CONSOLE:DEBUG_TO_LOG);	
      break;
    case 'f':
      strcpy(mps_librarydbd_options_p->db_filename, optarg);
      break;
    case 'p':
      strcpy(mps_librarydbd_options_p->persistent_db_filename, optarg);
      break;
    case 'm':
      strcpy(mps_librarydbd_options_p->media_pathname, optarg);
      break;
    default:
      usage(argv[0]);
      break;     
    }
  }

  

#ifdef DEBUG_ENABLED
  mps_debug_enabled = mps_librarydbd_options_p->debug_enabled;
  mps_debug_sink = mps_librarydbd_options_p->debug_sink;
#endif

  mps_librarydbd_tokens=&argv[optind];
  mps_librarydbd_num_tokens=argc-optind;

  /* command line getopt() arguments always take precedence, but if 
     none have been set, take the values from the XML files */

  if (!strcmp(mps_librarydbd_options_p->db_filename, "")) {
    strcpy(shmfile_param.name, "live_librarydb_file");
    rv = mps_constants_get_param(&shmfile_param);
    if (rv != MPS_SUCCESS) {
      goto OUT;
    }
    else {
      strcpy(mps_librarydbd_options_p->db_filename, 
	     shmfile_param.value); 
    }
  }

  if (!strcmp(mps_librarydbd_options_p->persistent_db_filename, "")) {
    strcpy(shmfile_param.name, "persistent_librarydb_file");
    rv = mps_constants_get_param(&shmfile_param);
    if (rv != MPS_SUCCESS) {
      goto OUT;
    }
    else {
      strcpy(mps_librarydbd_options_p->persistent_db_filename, 
	     shmfile_param.value); 
    }
  }

  if (!strcmp(mps_librarydbd_options_p->media_pathname, "")) {
    strcpy(shmfile_param.name, "media_dir__0");
    mps_configuration_get_param(&shmfile_param);
    if (rv != MPS_SUCCESS) {
      goto OUT;
    }
    else {
      strcpy(mps_librarydbd_options_p->media_pathname, 
	     shmfile_param.value); 
    }
  }

 OUT:
  MPS_DBG_PRINT("options structure:\n");
  MPS_DBG_PRINT("mps_librarydbd_options = {\n");
  MPS_DBG_PRINT("  debug_enabled = %d\n", 
		mps_librarydbd_options_p->debug_enabled);
  MPS_DBG_PRINT("  debug_sink = %d\n", mps_librarydbd_options_p->debug_sink);
  MPS_DBG_PRINT("  db_filename = %s\n", 
		mps_librarydbd_options_p->db_filename);
  MPS_DBG_PRINT("  persistent_db_filename = %s\n", 
		mps_librarydbd_options_p->persistent_db_filename);
  MPS_DBG_PRINT("  media_pathname = %s\n", 
		mps_librarydbd_options_p->media_pathname);
  MPS_DBG_PRINT("}\n");

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Couldn't initialize mps-librarydbd correctly.  Exiting.\n");
    exit(1);
  }
  else {
    mps_librarydbd_init();
    mps_librarydbd_mainloop();
  }				       
}
#endif // #ifndef UNIT_TEST

/* 
 * mps_librarydbd_init() - Initialize the sqlite3 database and 
 *                          other process attributes.
 */
int mps_librarydbd_init()
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int sqlite_rc, pid;
  sqlite3 *db;
  char *zErrMsg = 0;
  bool creating_new_db;
  char statusTableInitializationStatement[MAX_SQL_STATEMENT_LENGTH];
  char *tableCreationStatement = 
    "create table mps_library_titles ("
    "title_id int64 PRIMARY KEY, db_version int64, "
    "file_length int64, file_last_modification int64, "
    "parent_path_id int64, file_name text, file_type int, title text, "
    "album_id int64, artist_id int64, year int, genre text, "
    "time_length int, play_count int, last_played int64, "
    "ordinal_track_number int, bitrate int, sampling_rate int, "
    "channel_mode int, vbr_mode int, filesystem_id int64, "
    "FOREIGN KEY (album_id) REFERENCES mps_library_albums(album_id), "
    "FOREIGN KEY (artist_id) REFERENCES mps_library_artists(artist_id), "
    "FOREIGN KEY (parent_path_id) REFERENCES mps_library_file_path(id), "
    "FOREIGN KEY (filesystem_id) REFERENCES mps_library_filesystem_status(id) "
    "); "
    ""
    "create table mps_library_artists "
    "(artist_id int64 PRIMARY KEY, artist text); "
    ""
    "create table mps_library_albums "
    "(album_id int64 PRIMARY KEY, artist_id int64, album text, "
    "FOREIGN KEY (artist_id) REFERENCES mps_library_artists(artist_id) ); "
    ""
    "create table mps_library_file_path "
    "(id int64 PRIMARY KEY, parent_id int64, name text, db_version int64, "
    "filesystem_id int64, "
    "FOREIGN KEY (parent_id) REFERENCES mps_library_file_path(id), "
    "FOREIGN KEY (filesystem_id) REFERENCES mps_library_filesystem_status(id) ); "
    ""
    "create table mps_library_global_status "
    "(schema_version int64, scan_counter int64); "
    ""
    "create table mps_library_filesystem_status "
    "(id int64, size int64); "
    ""
    "CREATE UNIQUE INDEX update_lookup on mps_library_titles "
    "(file_length, file_last_modification, parent_path_id, file_name); ";

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  // initialize shared memory region
  if (mps_initialize_client() < MPS_SUCCESS) {
    exit(1);
  }

  // Set up a pointer to shared memory so that we can share with everyone
  state = &(mps_shm_region->library_status.librarydbd_state);

  *state = LIBRARYDBD_STATE_INIT;
  MPS_DBG_PRINT("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
  MPS_LOG("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));

  snprintf(statusTableInitializationStatement, MAX_SQL_STATEMENT_LENGTH,
	   "insert into mps_library_global_status "
	   "values(%d, 0); "
	   "insert into mps_library_filesystem_status "
	   "values(0, 0); ",
	   LIBRARYDB_SCHEMA_VERSION);
	   
  /* This is not really a time critical process, so give it the
     lowest priority. */
  pid = getpid();
  setpriority(PRIO_PROCESS,pid,19);

  // if persistent file exists, copy it to the "live" file
  if (strcmp(mps_librarydbd_options_p->persistent_db_filename, "")) {
    if (open(mps_librarydbd_options_p->persistent_db_filename, O_EXCL) != -1) {
      if (copy_unix_file(mps_librarydbd_options_p->persistent_db_filename,
			 mps_librarydbd_options_p->db_filename)
	  != COPYFILE_OK) {
	MPS_LOG("Couldn't copy persistent \"%s\" to live \"%s\"\n",
		mps_librarydbd_options_p->persistent_db_filename,
		mps_librarydbd_options_p->db_filename);
      }
    }
    else {
      MPS_LOG("Couldn't open persistent file \"%s\"\n", 
	      mps_librarydbd_options_p->persistent_db_filename);
    }
  }

  if (open(mps_librarydbd_options_p->db_filename, O_EXCL) == -1) {
    MPS_DBG_PRINT("File \"%s\" doesn't exist... creating and initializing it's database\n", 
		  mps_librarydbd_options_p->db_filename);

    /* initialize the database */
    creating_new_db = true;
    sqlite_rc = sqlite3_open(mps_librarydbd_options_p->db_filename, &db);
    if( sqlite_rc ){
      MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return MPS_LIBRARYDBD_FAILURE;
    }
    sqlite3_busy_timeout(db, SQLITE3_BUSY_TIMEOUT_MSEC);

    MPS_DBG_PRINT("Creating tables with statement:\n%s\n",
		  tableCreationStatement);
    sqlite_rc = sqlite3_exec(db, tableCreationStatement, NULL, 0, &zErrMsg);
    if( sqlite_rc != SQLITE_OK ){
      MPS_LOG("creating table - SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return MPS_LIBRARYDBD_FAILURE;
    } 

    MPS_DBG_PRINT("Initializing tables with statement:\n%s\n",
		  statusTableInitializationStatement);
    sqlite_rc = 
      sqlite3_exec(db, statusTableInitializationStatement, NULL, 0, &zErrMsg);
    if( sqlite_rc != SQLITE_OK ){
      MPS_LOG("Initializing library_status - error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
      return MPS_LIBRARYDBD_FAILURE;
    }
  }
  else {
    MPS_DBG_PRINT("File \"%s\" already exists...\n",
		  mps_librarydbd_options_p->db_filename);

    creating_new_db = false;
    sqlite_rc = sqlite3_open(mps_librarydbd_options_p->db_filename, &db);
    if( sqlite_rc ){
      MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return MPS_LIBRARYDBD_FAILURE;
    }
  }

  MPS_DBG_PRINT("Setting cache_size=0, synchronous=OFF\n");
  sqlite_rc = 
    sqlite3_exec(db, 
		 "PRAGMA cache_size=0; PRAGMA synchronous=OFF; "
		 "PRAGMA temp_store=2; ", 
		 NULL, 0, &zErrMsg);
  if ( sqlite_rc != SQLITE_OK ) {
    MPS_LOG("Setting cache_size - error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    return MPS_LIBRARYDBD_FAILURE;
  }
  else {
    sqlite3_busy_timeout(db, SQLITE3_BUSY_TIMEOUT_MSEC);
  }

  if (creating_new_db) {
    highestTitleId = MPS_MIN_TITLE_ID;
    highestArtistId = MPS_MIN_ARTIST_ID;
    highestAlbumId = MPS_MIN_ALBUM_ID;
    highestFilePathId = MPS_MIN_FILE_PATH_ID;
  }
  else {
    //Initialize it based on the maximum value of id stored in DB
    get_max_id
      (db, "title_id", "mps_library_titles", &highestTitleId);
    get_max_id
      (db, "album_id", "mps_library_albums", &highestAlbumId);
    get_max_id
      (db, "artist_id", "mps_library_artists", &highestArtistId);
    get_max_id
      (db, "id", "mps_library_file_path", &highestFilePathId);
  }

  sqlite3_close(db);

  *state = (creating_new_db ? 
	   LIBRARYDBD_STATE_CREATINGDB : 
	   LIBRARYDBD_STATE_WAITING);
  MPS_DBG_PRINT("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
  MPS_LOG("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);

  return rv;
}

void mps_librarydbd_mainloop()
{
  long newfree=0, oldfree=0;
  long tempLong;
  struct statfs mediaDiskStatfs;
  sqlite3 *db;
  char *zErrMsg = 0;
  int sqlite_rc;
  char real_path[2048];
  

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  sqlite_rc = sqlite3_open(mps_librarydbd_options_p->db_filename, &db);
  if( sqlite_rc ){
    MPS_LOG("Can't open database: %s\n", sqlite3_errmsg(db));
    exit(1);
  }
  sqlite3_busy_timeout(db, SQLITE3_BUSY_TIMEOUT_MSEC);

  realpath(mps_librarydbd_options_p->media_pathname, real_path);
  MPS_DBG_PRINT("real_path is %s\n", real_path);

  mps_librarydb_get_long(db, "mps_library_filesystem_status", 
			 "size", "rowid=1", &oldfree); 
  mps_librarydb_get_long(db, "mps_library_global_status", 
			 "scan_counter", "rowid=1", &dbVersion);

  while(1) {

    /* retrieve # of blocks free */ 
    statfs(real_path, &mediaDiskStatfs);
    newfree = mediaDiskStatfs.f_bfree;
    
    /* compare # of blocks that are free with the number that
       were free during the last iteration... if the size of
       the filesystem appears to have changed, scan for updates update */
#ifdef NO_LOOP_FOR_TESTING
    if (1) {
#else
    if(labs(oldfree - newfree) < 
       LIBRARYDB_FS_CHANGE_SIZE_THRESHOLD_BLOCKS) {
      MPS_DBG_PRINT("\"%s\" didn't change by > %d blocks (new=%ld, old=%ld).\n", 
		    real_path, LIBRARYDB_FS_CHANGE_SIZE_THRESHOLD_BLOCKS,
		    newfree, oldfree);
    }
    else {
#endif
      
      MPS_LOG("newfree=%ld, oldfree =%ld\n", newfree, oldfree); 
      if (*state != LIBRARYDBD_STATE_CREATINGDB) {

#ifndef NO_LOOP_FOR_TESTING
	wait_for_changes_to_settle(mps_librarydbd_options_p->media_pathname, 
				   &newfree);
#endif
	*state = LIBRARYDBD_STATE_UPDATINGDB;
	MPS_DBG_PRINT("Set *state = %s\n", 
		      MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
	MPS_LOG("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
      }

      numberTitlesAdded = numberTitlesDeleted = numberArtistsAdded =
	numberArtistsDeleted = numberAlbumsAdded = numberAlbumsDeleted = 
	numberFilePathsAdded = numberFilePathsDeleted = 0;
      
      if (*state != LIBRARYDBD_STATE_CREATINGDB) {
	sqlite_rc = sqlite3_exec(db, "BEGIN TRANSACTION; ", NULL, 0, &zErrMsg);
	if( sqlite_rc != SQLITE_OK ){
	  MPS_LOG("SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  exit(1);
	}
      }

      // Here is where we start the recursive search through the tree
      if (mps_librarydb_traverse_directory
	  (mps_librarydbd_options_p->media_pathname, 
	   db, 
	   MPS_MIN_FILE_PATH_ID)) {
	MPS_LOG("mediaDBd.traverse_directory() returned error\n");
      }

      if (*state != LIBRARYDBD_STATE_CREATINGDB) {
	sqlite_rc = sqlite3_exec(db, "END TRANSACTION; ", NULL, 0, &zErrMsg);
	if( sqlite_rc != SQLITE_OK ){
	  MPS_LOG("SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  exit(1);
	}
      }

      remove_stale_entries(db);
      
      MPS_DBG_PRINT("numberTitlesAdded = %d, numberTitlesDeleted = %d\n",
		     numberTitlesAdded, numberTitlesDeleted);
      MPS_DBG_PRINT("numberArtistsAdded = %d, numberArtistsDeleted = %d\n",
		     numberArtistsAdded, numberArtistsDeleted);
      MPS_DBG_PRINT("numberAlbumsAdded = %d, numberAlbumsDeleted = %d\n",
		     numberAlbumsAdded, numberAlbumsDeleted);
      MPS_DBG_PRINT("numberFilePathsAdded = %d, numberFilePathsDeleted = %d\n",
		     numberFilePathsAdded, numberFilePathsDeleted);

      
      if (numberTitlesAdded > 0 || numberTitlesDeleted > 0 ||
	  numberArtistsAdded > 0 || numberArtistsDeleted > 0 ||
	  numberAlbumsAdded > 0 || numberAlbumsDeleted > 0 ||
	  numberFilePathsAdded > 0 || numberFilePathsDeleted > 0) {
	MPS_LOG("DB actually changed: titles added=%ld, titles deleted=%ld,\n"
		"artists added=%ld, artists deleted=%ld,\n"
		"albums added=%ld, albums deleted=%ld,\n"
		"paths added=%ld, paths deleted=%ld\n",
		numberTitlesAdded, numberTitlesDeleted,
		numberArtistsAdded, numberArtistsDeleted,
		numberAlbumsAdded, numberAlbumsDeleted,
		numberFilePathsAdded, numberFilePathsDeleted);
	
	mps_shm_region->library_status.change_counter = dbVersion;


	/* save a persistent copy of the db (if -p was specified) */
	if (strcmp(mps_librarydbd_options_p->persistent_db_filename, "")) {
	  if (copy_unix_file(mps_librarydbd_options_p->db_filename,
			     mps_librarydbd_options_p->persistent_db_filename)
	      != COPYFILE_OK) {
	    MPS_LOG("Couldn't copy live \"%s\" to persistent \"%s\"\n",
		    mps_librarydbd_options_p->db_filename,
		    mps_librarydbd_options_p->persistent_db_filename);
	  }
	  else {
	    sync(); /* force write of all cached disk blocks */
	  }
	}
      }
      
      dbVersion++;

      mps_librarydb_set_long(db, "mps_library_filesystem_status", 
			     "size", "rowid=1", oldfree); 
      mps_librarydb_set_long(db, "mps_library_global_status", 
			     "scan_counter", "rowid=1", dbVersion);           
    }

    oldfree = newfree;

    if (*state != LIBRARYDBD_STATE_WAITING) {
      *state = LIBRARYDBD_STATE_WAITING;
      MPS_DBG_PRINT("Set *state = %s\n", 
		    MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
      MPS_LOG("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
    }
#ifdef NO_LOOP_FOR_TESTING
    exit(0);
#else
    sleep(10);
#endif
  }

  sqlite3_close(db);
  MPS_DBG_PRINT("Leaving %s() - WTF?\n", __func__);
}


int mps_librarydb_get_long(sqlite3 *db, char *table, char *column_name, 
			    char *where_clause, long *value)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int sqlite_rc;
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  sqlite3_stmt *stmt = NULL;

  MPS_DBG_PRINT("Entering %s(0x%08x, \"%s\", \"%s\", \"%s\", 0x%08x)\n", 
		 __func__, db, table, column_name, where_clause, value);

  if (where_clause == NULL) {
    snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
             "select %s from %s",
             column_name, table);
  }
  else {
    snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
             "select %s from %s where %s",
             column_name, table, where_clause);
  }
  MPS_DBG_PRINT("Calling sqlite3_prepare_v2 on \"%s\"\n", sqlStatement);

  sqlite_rc = sqlite3_prepare_v2(db, sqlStatement, -1, &stmt, 0);
  
  if(sqlite_rc != SQLITE_OK ) { 
    MPS_LOG("SQL error: %d\n", sqlite_rc);
    rv = MPS_LIBRARYDBD_FAILURE;
    goto OUT;
  }

  sqlite_rc = sqlite3_step(stmt);

  /* check for errors and then retry until returns SQL_ROW */
  if(sqlite_rc == SQLITE_ROW ) {
    *value = sqlite3_column_int64(stmt, 0); 
    MPS_DBG_PRINT("*value = %d\n", *value);
  }
  else {
    rv = MPS_LIBRARYDBD_NO_RESULTS;
    goto OUT;
  }

 OUT:
  sqlite3_finalize(stmt);
 
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_librarydb_set_long(sqlite3 *db, char *table, char *column_name, 
			    char *where_clause, long value)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int sqlite_rc;
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  char *zErrMsg = 0;

  MPS_DBG_PRINT("Entering %s(0x%08x, \"%s\", \"%s\", \"%s\", %d)\n", 
		__func__, db, table, column_name, where_clause, value);

  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "update %s set %s=%ld where %s",
	   table, column_name, value, where_clause);
  
  sqlite_rc = 
    sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);  

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static void usage(char *command_name)
{
   printf("Usage:\n");
   printf("  %s [ -d 'c'|'l' ] [ -f <filename> ] [ -p <filename> ]\n", 
	  command_name);
   printf("     -d : Output debug to console or log\n");
   printf("     -f : Full path filename for live database file\n");
   printf("     -p : Full path filename for persistent database file\n");
   printf("     -m : Full path directory name for media library\n");
   printf("\n");
   exit(1);
}

/*
  remove_stale_entries() -
  
  Removes all of the entries in mps_library_titles where 
  scan_counter field is < dbVersion and returns the number 
  of entries that were removed.  Also scans for and deletes
  entries in mps_library_artists and mps_library_albums
  that have been orphaned because of title deletions.
*/

static int remove_stale_entries(sqlite3 *db)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int sqlite_rc;
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  sqlite3_stmt *stmt = NULL;
  char *zErrMsg = 0;
  long tempLong;
  char tempString[128];

  id_ll_node_t *album_list_head, *album_list_current;
  id_ll_node_t *artist_list_head, *artist_list_current;

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  /* retrieve the stale title entries and place them in a linked
     list that will later be used to determine and delete 
     any */
  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "select distinct album_id from mps_library_titles "
	   "where db_version<%ld",
	   dbVersion);
  place_ids_in_list(db, sqlStatement, &album_list_head);

  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "select distinct artist_id from mps_library_titles "
	   "where db_version<%ld",
	   dbVersion);
  place_ids_in_list(db, sqlStatement, &artist_list_head);

  /* delete the stale titles */
  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "delete from mps_library_titles where db_version<%ld",
	   dbVersion);
  MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
  
  sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
  if( sqlite_rc != SQLITE_OK ){
    MPS_LOG("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    rv = MPS_LIBRARYDBD_FAILURE;
    goto OUT;
  }
  else {
    numberTitlesDeleted = sqlite3_changes(db);
  }

  /* delete the stale file_paths */
  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "delete from mps_library_file_path where db_version<%ld",
	   dbVersion);
  MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
  
  sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
  if( sqlite_rc != SQLITE_OK ){
    MPS_LOG("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    rv = MPS_LIBRARYDBD_FAILURE;
    goto OUT;
  }
  else {
    numberFilePathsDeleted = sqlite3_changes(db);
  }

  memset(sqlStatement, 0, MAX_SQL_STATEMENT_LENGTH);
  /* delete any orphaned _artists and _albums entries */
  for (artist_list_current = artist_list_head;
       artist_list_current != NULL;
       artist_list_current = artist_list_current->next) {
    snprintf(tempString, MAX_SQL_STATEMENT_LENGTH,
	     "artist_id=%ld", artist_list_current->id);
    mps_librarydb_get_long(db, "mps_library_titles", "COUNT(*)", 
			    tempString, &tempLong);
    MPS_DBG_PRINT("Found %d titles with artist_id=%d\n",
		   tempLong, artist_list_current->id);
    if (tempLong == 0) {
      numberArtistsDeleted++;
      snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	       "delete from mps_library_artists where artist_id=%ld; ",
	       artist_list_current->id);
      MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
      sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
      if( sqlite_rc != SQLITE_OK ){
	MPS_LOG("SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
	rv = MPS_LIBRARYDBD_FAILURE;
	goto OUT;
      }    
    }
  }

  for (album_list_current = album_list_head;
       album_list_current != NULL;
       album_list_current = album_list_current->next) {
    snprintf(tempString, MAX_SQL_STATEMENT_LENGTH,
	     "album_id=%ld", album_list_current->id);
    mps_librarydb_get_long(db, "mps_library_titles", "COUNT(*)", 
			   tempString, &tempLong);    
    MPS_DBG_PRINT("Found %d titles with album_id=%d\n",
		  tempLong, album_list_current->id);
    if (tempLong == 0) {
      numberAlbumsDeleted++;
      snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	       "delete from mps_library_albums where album_id=%ld; ",
	       album_list_current->id);

      MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
      sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
      if( sqlite_rc != SQLITE_OK ){
	MPS_LOG("SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
	rv = MPS_LIBRARYDBD_FAILURE;
	goto OUT;
      }    
    }
  }
  
 OUT:
  free_id_list(artist_list_head);
  free_id_list(album_list_head);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int place_ids_in_list(sqlite3 *db, char *query, id_ll_node_t **list)
{
  int sqlite_rc;
  id_ll_node_t *head, *current;
  sqlite3_stmt *stmt = NULL;

  head = current = NULL;

  sqlite_rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
  if(sqlite_rc != SQLITE_OK ) { 
    MPS_LOG("SQL error: %d\n", sqlite_rc);
    return MPS_LIBRARYDBD_FAILURE;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {    
    if (head == NULL) {
      current = malloc(sizeof(id_ll_node_t));
      head = current;
    }
    else {
      current->next = malloc(sizeof(id_ll_node_t));
      current = current->next;
    }
    current->id = sqlite3_column_int64(stmt, 0);
    current->next = NULL;
  }

  *list = head;  
  return MPS_LIBRARYDBD_SUCCESS;
}

static int free_id_list(id_ll_node_t *list)
{
  id_ll_node_t *current = list;
  id_ll_node_t *next = current;

  while (current != NULL) {
    next = current->next;
    free(current);
    current = next;
  }
}

/*
  gets max(<id element>) from  <table> 
 */
static int get_max_id(sqlite3 *db, char *id_name, 
			     char *table_name, long *id_return_value)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int sqlite_rc;
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  sqlite3_stmt *stmt = NULL;

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, id_return_value);

  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	   "select max(%s) from %s",
	   id_name, table_name);

  sqlite_rc = sqlite3_prepare_v2(db, sqlStatement, -1, &stmt, 0);
  
  if(sqlite_rc != SQLITE_OK ) { 
    MPS_LOG("SQL error: %d\n", sqlite_rc);
    return MPS_LIBRARYDBD_FAILURE;
  }

  sqlite_rc = sqlite3_step(stmt);

  /* check for errors and then retry until returns SQL_ROW */
  if(sqlite_rc == SQLITE_ROW ) {
    *id_return_value = sqlite3_column_int64(stmt, 0); 
    MPS_DBG_PRINT("*id_return_value = %d\n", *id_return_value);
  }
  else {
    MPS_LOG("Couldn't sqlite3_step() statement \"%s\"\n", sqlStatement);
    return MPS_LIBRARYDBD_FAILURE;
  }

  sqlite3_finalize(stmt);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);

  return rv;
}

/*
  wait_for_changes_to_settle -
  Waits until filessytems size has stayed stable for at least
  LIBRARYDB_FS_CHANGE_SETTLING_TIME_USEC milliseconds.
  
  Returns (by reference), the size after the changes have
  settled.
 */

static int wait_for_changes_to_settle(char *fs_mount_name, long *free) 
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  long free_previous_sample;
  struct statfs tmpstatfs;

  MPS_DBG_PRINT("Entering %s(%s, *%d)\n", __func__, fs_mount_name, *free);
    
  *state = LIBRARYDBD_STATE_SETTLING;
  MPS_DBG_PRINT("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));
  MPS_LOG("Set *state = %s\n", MPS_LIBRARYDB_STATE_T_TO_STRING(*state));

  do {
    free_previous_sample = *free;
    usleep(LIBRARYDB_FS_CHANGE_SETTLING_TIME_USEC);
    statfs(fs_mount_name, &tmpstatfs);
    *free = tmpstatfs.f_bfree;
  }
  while (free_previous_sample != *free);

  MPS_DBG_PRINT("Leaving %s() - return value %d, *free = %d\n", 
		 __func__, rv, *free);
  return rv;
}

static int copy_unix_file(const char *source, const char *target) 
{
  int rc = COPYFILE_OK;
  FILE *fpin = fopen(source, "rb");

  if(fpin != NULL) {
    FILE *fpout = fopen(target, "wb");

    if(fpout != NULL) {
      int ch = 0;

      while((ch = getc(fpin)) != EOF) {
        putc(ch, fpout);
      }

      if(ferror(fpin)) {
        rc |= COPYFILE_READ_ERROR;
      }

      if(ferror(fpout)) {
        rc |= COPYFILE_WRITE_ERROR;
      }

      if(fclose(fpout) != 0) {
        rc |= COPYFILE_OUTCLOSE_ERROR;
      }
    }
    else {
      rc |= COPYFILE_OUTOPEN_ERROR;
    }

    if(fclose(fpin) != 0) {
      rc |= COPYFILE_INCLOSE_ERROR;
    }
  }
  else {
    rc |= COPYFILE_INOPEN_ERROR;
  }
  return rc;
} 

char *genre_table[] =
{
	"Blues",
	"Classic Rock",
	"Country",
	"Dance",
	"Disco",
	"Funk",
	"Grunge",
	"Hip-Hop",
	"Jazz",
	"Metal",
	"New Age",
	"Oldies",
	"Other",
	"Pop",
	"R&B",
	"Rap",
	"Reggae",
	"Rock",
	"Techno",
	"Industrial",
	"Alternative",
	"Ska",
	"Death Metal",
	"Pranks",
	"Soundtrack",
	"Euro-Techno",
	"Ambient",
	"Trip-Hop",
	"Vocal",
	"Jazz+Funk",
	"Fusion",
	"Trance",
	"Classical",
	"Instrumental",
	"Acid",
	"House",
	"Game",
	"Sound Clip",
	"Gospel",
	"Noise",
	"AlternRock",
	"Bass",
	"Soul",
	"Punk",
	"Space",
	"Meditative",
	"Instrumental Pop",
	"Instrumental Rock",
	"Ethnic",
	"Gothic",
	"Darkwave",
	"Techno-Industrial",
	"Electronic",
	"Pop-Folk",
	"Eurodance",
	"Dream",
	"Southern Rock",
	"Comedy",
	"Cult",
	"Gangsta",
	"Top 40",
	"Christian Rap",
	"Pop/Funk",
	"Jungle",
	"Native American",
	"Cabaret",
	"New Wave",
	"Psychadelic",
	"Rave",
	"Showtunes",
	"Trailer",
	"Lo-Fi",
	"Tribal",
	"Acid Punk",
	"Acid Jazz",
	"Polka",
	"Retro",
	"Musical",
	"Rock & Roll",
	"Hard Rock",
	"Folk",
	"Folk/Rock",
	"National folk",
	"Swing",
	"Fast-fusion",
	"Bebob",
	"Latin",
	"Revival",
	"Celtic",
	"Bluegrass",
	"Avantgarde",
	"Gothic Rock",
	"Progressive Rock",
	"Psychedelic Rock",
	"Symphonic Rock",
	"Slow Rock",
	"Big Band",
	"Chorus",
	"Easy Listening",
	"Acoustic",
	"Humour",
	"Speech",
	"Chanson",
	"Opera",
	"Chamber Music",
	"Sonata",
	"Symphony",
	"Booty Bass",
	"Primus",
	"Porn Groove",
	"Satire",
	"Slow Jam",
	"Club",
	"Tango",
	"Samba",
	"Folklore",
	"Ballad",
	"Powder Ballad",
	"Rhythmic Soul",
	"Freestyle",
	"Duet",
	"Punk Rock",
	"Drum Solo",
	"A Capella",
	"Euro-House",
	"Dance Hall",
	"Goa",
	"Drum & Bass",
	"Club House",
	"Hardcore",
	"Terror",
	"Indie",
	"BritPop",
	"NegerPunk",
	"Polsk Punk",
	"Beat",
	"Christian Gangsta",
	"Heavy Metal",
	"Black Metal",
	"Crossover",
	"Contemporary C",
	"Christian Rock",
	"Merengue",
	"Salsa",
	"Thrash Metal",
	"Anime",
	"JPop",
	"SynthPop",
	"Unknown",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	""
};


