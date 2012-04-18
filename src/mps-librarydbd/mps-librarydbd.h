/*
 * Name: mps-librarydbd.h
 *
 * Purpose: This file contains shared prototypes and definitions
 *          related to mps-librarydbd.
 *
 */
#ifndef MPS_LIBRARYDBD_H
#define MPS_LIBRARYDBD_H

#include <stdio.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <sqlite3.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include "libmpsserver-private.h" 

#define MPS_LIBRARYDBD_SUCCESS     0
#define MPS_LIBRARYDBD_FAILURE    -1
#define MPS_LIBRARYDBD_NO_RESULTS -2

#define LIBRARYDB_SCHEMA_VERSION 1

#define MAX_PATHNAME_LENGTH 1024

#define SQLITE3_BUSY_TIMEOUT_MSEC 10000
#define LIBRARYDB_FS_CHANGE_SETTLING_TIME_USEC 5000000
#define LIBRARYDB_FS_CHANGE_SIZE_THRESHOLD_BLOCKS 5
 

// options structure
typedef struct mps_librarydbd_options {
  int            debug_enabled;
  int            debug_sink;
  char           db_filename[MAX_PATHNAME_LENGTH];
  char           persistent_db_filename[MAX_PATHNAME_LENGTH];
  // Eventually this will be a dynamic _set_ of share directories
  char           media_pathname[MAX_PATHNAME_LENGTH];
} mps_librarydbd_options_t;
extern mps_librarydbd_options_t *mps_librarydbd_options_p;

// function prototypes
int mps_librarydbd_init();
void mps_librarydbd_mainloop();
int copy_string_without_edge_whitespace(char *, char *, int);
int mps_librarydb_get_long(sqlite3 *db, char *table, char * column_name, 
			    char *where_clause, long *value);
int mps_librarydb_set_long(sqlite3 *db, char *table, char *column_name, 
			    char *where_clause, long value);
int mps_librarydb_traverse_directory(char *directoryName, sqlite3 *db, 
				      long caller_file_path_id);

// externs
extern long highestTitleId;
extern long highestAlbumId;
extern long highestArtistId;
extern long highestFilePathId;
extern mps_librarydbd_state_t *state;
extern char *genre_table[];
extern long dbVersion, numberTitlesAdded, numberTitlesDeleted,
  numberArtistsAdded, numberArtistsDeleted, numberAlbumsAdded,
  numberAlbumsDeleted, numberFilePathsAdded, numberFilePathsDeleted;



// utility macros
#define IS_WHITESPACE(x) (x == ' ' || x == '\t')
#define IS_UNKNOWN(x) ((x[0]=='?'&&x[1]=='\0')||x[0]=='\0')

#endif /* MPS_LIBRARYDBD_H */
