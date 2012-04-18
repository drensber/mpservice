#include "mps-librarydbd.h"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include "libmpg123/mpg123.h"

static mps_library_element_type_t get_file_type(char*);
static void copy_and_sqlite_escape(char *, char *, int); 
static void sanitize_and_copy (char *, char *, int);
static int get_track_info(char *fullPath, 
			  mps_library_track_info_t *track_info);
static int get_track_info_mp3(char *fullPath, 
			      mps_library_track_info_t *track_info);
static int copy_genre_and_convert_if_necessary(char *, char *);
static long update_artist_table(char *artist, sqlite3 *db);
static long update_album_table(char *album, long artistId, sqlite3 *db);

int mps_librarydb_traverse_directory(char *directoryName, sqlite3 *db,
				     long caller_file_path_id) 
{
  DIR *directoryStreamPointer;
  struct dirent *directoryElementPointer;
  struct stat statStructure;
  char filePathCopy[MPS_MAX_LIBRARY_PATH_LENGTH];
  char fullFilePath[MPS_MAX_LIBRARY_PATH_LENGTH];
  char escapedFileName[MPS_MAX_LIBRARY_PATH_LENGTH];
  char escapedTitle[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  char escapedArtist[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  char escapedAlbum[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  char escapedGenre[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  char escapedDname[NAME_MAX+1];
  char *zErrMsg = 0;
  int i, sqlite_rc, gti_rc, rv = MPS_LIBRARYDBD_SUCCESS;
  int updated_track_count, updated_file_path_count;
  int tempErrno;
  long artistId, albumId, filePathId;
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  char whereClause[MAX_SQL_STATEMENT_LENGTH];
  mps_library_track_info_t track_info;

  MPS_DBG_PRINT("Entering %s(%s, 0x%08x, %d)\n", __func__, 
		directoryName, (unsigned)db, caller_file_path_id);

  if ((directoryStreamPointer = opendir(directoryName))
      == NULL) {
    MPS_LOG("Error: Couldn't open \"%s\"\n", directoryName);
    goto OUT;
  }
                  
  while (1) {
    errno = 0;
    if ((directoryElementPointer = readdir(directoryStreamPointer))
        == NULL) {
      tempErrno=errno;
      if (tempErrno ==0) {
        break;
      }
      else {
        MPS_LOG("readdir() returned NULL with errno==%d\n", tempErrno);
        sleep(10);
        continue;
      }
    }
    MPS_DBG_PRINT("Top of while() loop. d_name=\"%s\"\n", 
		  directoryElementPointer->d_name);

    /* zero the track_info structure */
    memset(&track_info, 0, sizeof(mps_library_track_info_t));

    //Construct full path name in source directory 
    // could I just use realpath()?
    strcpy(fullFilePath, directoryName);
    strcat(fullFilePath, "/");
    strcat(fullFilePath, directoryElementPointer->d_name);
    
    track_info.parent_path_id = caller_file_path_id;
    strncpy(track_info.file_name, directoryElementPointer->d_name,
	    MPS_MAX_TRACK_INFO_FIELD_LENGTH);

    // escape the ' characters for sqlite's benefit
    copy_and_sqlite_escape(escapedFileName, 
                           track_info.file_name, 
                           MPS_MAX_LIBRARY_PATH_LENGTH); 
    
    stat(fullFilePath, &statStructure);
    
    if (S_ISREG(statStructure.st_mode)
	&& *(directoryElementPointer->d_name) != '.') {
      MPS_DBG_PRINT("Is a regular file\n");
      
      track_info.file_type = get_file_type(directoryElementPointer->d_name);
      if (track_info.file_type == MPS_LIBRARY_UNKNOWN) {
	MPS_DBG_PRINT("\"%s\" has an unrecognized file type\n", 
		      directoryElementPointer->d_name);
	continue;
      }

      track_info.file_length = statStructure.st_size;
      track_info.file_last_modification = statStructure.st_mtime;

      if (*state == LIBRARYDBD_STATE_UPDATINGDB) {
	
	snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH, 
		 "UPDATE mps_library_titles SET db_version=%ld "
		 "WHERE file_length=%ld AND file_last_modification=%ld "
		 "AND parent_path_id=%ld AND file_name='%s'; ",
		 /* "LIMIT 1; ", -- this might work if I build from 
		    non-"amalgamation" source */
		 dbVersion, track_info.file_length, 
		 track_info.file_last_modification,  
		 caller_file_path_id, escapedFileName);

	MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
	sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
	
	if( sqlite_rc != SQLITE_OK ){
	  MPS_LOG("SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  return MPS_LIBRARYDBD_FAILURE;
	}
	else {
	  updated_track_count = sqlite3_changes(db);
	}

	if (updated_track_count > 0) {
	  MPS_DBG_PRINT("File \"%s\" doesn't appear to have changed, updated scan_counter = %d\n", 
			fullFilePath, dbVersion);
	}
      }

      if (*state == LIBRARYDBD_STATE_CREATINGDB ||
	  (*state == LIBRARYDBD_STATE_UPDATINGDB && updated_track_count == 0)){
	
	gti_rc = get_track_info(fullFilePath, &track_info);
	if (gti_rc != MPS_LIBRARYDBD_SUCCESS) {
	  MPS_DBG_PRINT("get_track_info() returned MPS_LIBRARYDBD_FAILURE\n");
	  // Don't continue and add entry if get_track_info failed.
	  continue; 
	}

	MPS_DBG_PRINT("Adding file \"%s\"\n", fullFilePath);
	highestTitleId++;

        sanitize_and_copy (escapedTitle, 
                           track_info.title, 
                           MPS_MAX_TRACK_INFO_FIELD_LENGTH);
        if (IS_UNKNOWN(escapedTitle)) {
          sprintf(escapedTitle, "Unknown Title (%s)", escapedFileName);
        }

        sanitize_and_copy (escapedAlbum, 
                           track_info.album, 
                           MPS_MAX_TRACK_INFO_FIELD_LENGTH);
        if (IS_UNKNOWN(escapedAlbum)) {
          strcpy(escapedAlbum, "Unknown Album");
        }

        sanitize_and_copy (escapedArtist, 
                           track_info.artist, 
                           MPS_MAX_TRACK_INFO_FIELD_LENGTH); 
        if (IS_UNKNOWN(escapedArtist)) {
          strcpy(escapedArtist, "Unknown Artist");
        }

	copy_and_sqlite_escape(escapedGenre, 
                               track_info.genre, 
                               MPS_MAX_TRACK_INFO_FIELD_LENGTH); 
	
	/* TODO: figure out how to really get this */
	track_info.ordinal_track_number = 1;
	
	/* update artist table */
	artistId = update_artist_table(escapedArtist, db);
	
	/* update albums table */
	albumId = update_album_table(escapedAlbum, artistId, db);

	snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH, 
		 "insert into mps_library_titles "
		 "values("
		 "%ld, "   // title_id int64
		 "%ld, "   // db_version int64
		 "%ld, "   // file_length int64
		 "%ld, "   // file_last_modification int64
		 "%ld, "   // parent_path_id int64
		 "'%s', "  // file_name text
		 "%d, "    // file_type int
		 "'%s', "  // title text
		 "%ld, "   // album_id int64
		 "%ld, "   // artist_id int64
		 "%d, "    // year int
		 "'%s', "  // genre text
		 "%d, "    // time_length int
		 "%d, "    // play_count int
		 "%ld, "   // last_played int64
		 "%d, "    // ordinal_track_number int
		 "%d, "    // bitrate int
		 "%d, "    // sampling_rate int
		 "%d, "    // channel_mode int
		 "%d, "    // vbr_mode int
		 "%ld "    // filesystem_id int64
		 "); ",
		 highestTitleId, dbVersion,
		 track_info.file_length, track_info.file_last_modification,
		 caller_file_path_id, escapedFileName, track_info.file_type,
		 escapedTitle, albumId, artistId,
		 track_info.year, escapedGenre, track_info.time_length, 
		 track_info.play_count, track_info.last_played, 
		 track_info.ordinal_track_number, track_info.bitrate,
		 track_info.sampling_rate, track_info.channel_mode, 
		 track_info.vbr_mode, 0L /*fs_id hardcoded */);

	MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);	
	sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
	
	if( sqlite_rc != SQLITE_OK ){
	  MPS_LOG("SQL error: %s\n", zErrMsg);
	  sqlite3_free(zErrMsg);
	  highestTitleId--;
	  return MPS_LIBRARYDBD_FAILURE;
	}
	else {
	  numberTitlesAdded++;
	}
      }
    }
    else if (S_ISDIR(statStructure.st_mode)) {
      MPS_DBG_PRINT("Is a directory\n");

      copy_and_sqlite_escape(escapedDname, 
                             directoryElementPointer->d_name, 
                             MPS_MAX_LIBRARY_PATH_LENGTH); 	  
      
      if (strcmp(directoryElementPointer->d_name, ".") &&
	  strcmp(directoryElementPointer->d_name, "..")) {	  

	if (*state == LIBRARYDBD_STATE_UPDATINGDB) {	  
	  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
		   "update mps_library_file_path set db_version=%ld "
		   "where parent_id=%ld and name='%s'",
		   dbVersion, caller_file_path_id, escapedDname);
	  
	  MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
	  sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);

	  if( sqlite_rc != SQLITE_OK ){
	    MPS_LOG("SQL error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    return MPS_LIBRARYDBD_FAILURE;
	  }
	  else {
	    updated_file_path_count = sqlite3_changes(db);
	  }
	  if (updated_file_path_count > 0) {
	    snprintf(whereClause, MAX_SQL_STATEMENT_LENGTH,
		     "parent_id=%ld and name='%s'",
		     caller_file_path_id, escapedDname);
	    rv = mps_librarydb_get_long(db, "mps_library_file_path", "id",
					 whereClause, &filePathId);
	    if (rv < MPS_LIBRARYDBD_SUCCESS) {
	      goto OUT;
	    }
	  }
	}
	if (*state == LIBRARYDBD_STATE_CREATINGDB ||
	    (*state == LIBRARYDBD_STATE_UPDATINGDB && 
	     updated_file_path_count == 0)){
	  filePathId = ++highestFilePathId;
	  snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH, 
		   "insert into mps_library_file_path "
		   "values("
		   "%ld, "   // id int64
		   "%ld, "   // parent_id int64
		   "'%s', "  // name text
		   "%ld, "   // db_version int64
		   "%ld "    // filesystem_id int64
		   "); ",
		   highestFilePathId, caller_file_path_id,
		   escapedDname, dbVersion, 0L /* fs_id hard coded */);

	  MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);	
	  sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
	  if( sqlite_rc != SQLITE_OK ){
	    MPS_LOG("SQL error: %s\n", zErrMsg);
	    sqlite3_free(zErrMsg);
	    highestFilePathId--;
	    rv = MPS_LIBRARYDBD_FAILURE;
	    goto CLOSE_OUT;
	  }
	  else {
	    numberFilePathsAdded++;
	  }
	}
	MPS_DBG_PRINT("Adding directory \"%s\"\n", fullFilePath); 
	mps_librarydb_traverse_directory(fullFilePath, db, filePathId);
      }
    }
  }

 CLOSE_OUT:  
  closedir(directoryStreamPointer);    
 OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static mps_library_element_type_t get_file_type(char* filename)
{
  mps_library_element_type_t rv = MPS_LIBRARY_UNKNOWN;

  MPS_DBG_PRINT("Entering %s(%s)\n", __func__, filename);
  if (strcasestr(filename, ".mp3") != (int)NULL) {
    rv = MPS_LIBRARY_MP3_FILE;
  }
  MPS_DBG_PRINT("Leaving %s() - return value %s\n", 
		__func__, MPS_LIBRARY_ELEMENT_TYPE_TO_STRING(rv));

  return rv;
} 


static void copy_and_sqlite_escape(char *to, char *from, int len)
{
  int i, j;

  MPS_DBG_PRINT("Entering %s(0x%08x, %s, %d)\n", 
		__func__, (unsigned int) to, from, len);

  for (i = 0, j = 0; i < len; i++) {
    to[j] = from[i];
    if (from[i] == '\'') {
      to[++j] = '\'';
    }
    j++;
    if (from[i] == '\0') {
      break;
    }
  }

  MPS_DBG_PRINT("Leaving %s()\n", __func__);  
}


static void sanitize_and_copy (char *to, char *from, int len)
{
  int i, nb;
  char *c;
  char *current, *new_current;

  MPS_DBG_PRINT("Entering %s(0x%08x, %s, %d)\n", 
		__func__, (unsigned int) to, from, len);

  /* escape ' characters to keep sqlite happy */
  copy_and_sqlite_escape(to, from, len);

  /* if any invalid utf-8 detected, replace it with a '?' and
     end the string */
  for (c = to ; 
       (*c != '\0' && (c - to) < len) ;  
       c += (nb + 1)) {
    if (! (*c & 0x80)) {
      if ((*c > 0x00 && *c < 0x20) || *c == 0x7f) 
        {
          *c = '?';
          *(c + 1) = '\0';
          break;
        } 
      nb = 0;
    }
    else {
      if ((*c & 0xc0) == 0x80) {
        *c = '?';
        *(c + 1) = '\0';
        break;
      } 
      else if ((*c & 0xe0) == 0xc0) nb = 1;
      else if ((*c & 0xf0) == 0xe0) nb = 2;
      else if ((*c & 0xf8) == 0xf0) nb = 3;
      else if ((*c & 0xfc) == 0xf8) nb = 4;
      else if ((*c & 0xfe) == 0xfc) nb = 5;
      else {
        *c = '?';
        *(c + 1) = '\0';
        break;
      }
      for (i = 1; i < (nb + 1); i++) {
	if ((*(c + i) & 0xc0) != 0x80) {
          *c = '?';
          *(c + 1) = '\0';
          break;
	}
      }
    }
  }

  /* remove leading, trailing, and multiple whitespace */
  current = to;
  while (IS_WHITESPACE(*current) && ((current - to) < len)) current++;

  for (new_current = to; 
       (*current != '\0' && (current - to) < len); 
       new_current++, current++) {
    *new_current = *current;
    if (IS_WHITESPACE(*current)) 
      while(IS_WHITESPACE(*(current + 1)))
        current++;
  }
  
  if (IS_WHITESPACE(*(new_current - 1)))
    *(new_current -1) = '\0';
  else
    *new_current = '\0';

  MPS_DBG_PRINT("Leaving %s()\n", __func__);  
}


/* expects track_info->file_type to be filled in 
   prior to calling.   Fills in fields title, album, artist, year, genre, 
   and time.
*/
static int get_track_info(char *fullPath, mps_library_track_info_t *track_info)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int) track_info);

  switch (track_info->file_type) {
  case MPS_LIBRARY_MP3_FILE:
    rv = get_track_info_mp3(fullPath, track_info); 
    break;
  default:
    rv = MPS_LIBRARYDBD_FAILURE;    
  }

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);

  return rv;
}

static int get_track_info_mp3(char *fullPath, 
			      mps_library_track_info_t *track_info)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  mpg123_handle* m;
  mpg123_id3v1 *v1;
  mpg123_id3v2 *v2;  
  int meta;
  struct mpg123_frameinfo finfo;
  int err;
  int tmp;
  long total_samples;

  MPS_DBG_PRINT("Entering %s(0x%08x)\n", __func__, (unsigned int) track_info);

  mpg123_init();
  m = mpg123_new(NULL, &err);

  if (err != MPG123_OK) {
    MPS_DBG_PRINT("Call to mpg123_new resulted in error %d\n", err);
    rv = MPS_LIBRARYDBD_FAILURE;
    goto ERR_OUT;
  }

  if(mpg123_open_64(m, fullPath) != MPG123_OK) {
    MPS_DBG_PRINT("Cannot open %s: %s\n", 
	      fullPath, mpg123_strerror(m));
    rv = MPS_LIBRARYDBD_FAILURE;
    goto ERR_OUT;
  } 

  err = mpg123_scan(m);
  if (err != MPG123_OK) {
    MPS_DBG_PRINT("Call to mpg123_scan failed.\n");
    rv = MPS_LIBRARYDBD_FAILURE;
  }
  else {
    meta = mpg123_meta_check(m);
    if(!(meta & MPG123_ID3 && mpg123_id3(m, &v1, &v2) == MPG123_OK)) {
      MPS_DBG_PRINT("Nothing found for %s.\n", fullPath);
      // Don't return MPS_LIBRARYDBD_FAILURE... may just lack metadata;
    }
    else {
      if(v2 != NULL) {
	MPS_DBG_PRINT("\n");
	if (v2->title != NULL && v2->title->fill)
	  copy_string_without_edge_whitespace(track_info->title, 
					      v2->title->p, v2->title->fill);
	MPS_DBG_PRINT("\n");
	if (v2->album != NULL && v2->album->fill)
	  copy_string_without_edge_whitespace(track_info->album, 
					      v2->album->p, v2->album->fill);
	MPS_DBG_PRINT("\n");
	if (v2->artist != NULL && v2->artist->fill)
	  copy_string_without_edge_whitespace(track_info->artist, 
					      v2->artist->p, v2->artist->fill);
	MPS_DBG_PRINT("\n");
	if (v2->year != NULL && v2->year->fill)
	  track_info->year = atoi(v2->year->p);
	MPS_DBG_PRINT("\n");
	if (v2->genre != NULL && v2->genre->fill)
	  copy_genre_and_convert_if_necessary(track_info->genre, v2->genre->p);
      }
      if(v1 != NULL) {
	MPS_DBG_PRINT("\n");
	if (v2->title == NULL)
	  copy_string_without_edge_whitespace(track_info->title, v1->title, 60);
	MPS_DBG_PRINT("\n");
	if (v2->album == NULL)
	  copy_string_without_edge_whitespace(track_info->album, v1->album, 60);
	MPS_DBG_PRINT("\n");
	if (v2->artist == NULL)
	  copy_string_without_edge_whitespace(track_info->artist, 
					      v1->artist, 60);
	MPS_DBG_PRINT("\n");
	if (v2->year == NULL)
	  track_info->year = atoi(v1->year);
	MPS_DBG_PRINT("\n");
	if (v2->genre == NULL)
	  strcpy(track_info->genre, genre_table[v1->genre]);
      }
    }      
    tmp = mpg123_framebyframe_next(m);
    if (tmp == MPG123_DONE) {
      MPS_DBG_PRINT("Error: Received MPG123_DONE position while just decoding the header");
      rv = MPS_LIBRARYDBD_FAILURE;
    }
    else {
      MPS_DBG_PRINT("Calling mpg123_info.\n");
      mpg123_info(m, &finfo);
      
      MPS_DBG_PRINT("Setting bitrate, etc.\n");
      track_info->bitrate = finfo.bitrate;
      track_info->sampling_rate = (int)(finfo.rate);
      track_info->channel_mode = 
	((finfo.mode==MPG123_M_STEREO)?MPS_CHANNEL_MODE_STEREO:
	 ((finfo.mode==MPG123_M_JOINT)?MPS_CHANNEL_MODE_JOINT_STEREO:
	  ((finfo.mode==MPG123_M_DUAL)?MPS_CHANNEL_MODE_DUAL_CHANNEL:
	   ((finfo.mode==MPG123_M_MONO)?MPS_CHANNEL_MODE_MONO:
	    MPS_CHANNEL_MODE_UNKNOWN))));
      track_info->vbr_mode = 
	((finfo.vbr==MPG123_CBR)?MPS_VBR_MODE_CONSTANT:
	 ((finfo.vbr==MPG123_VBR)?MPS_VBR_MODE_VARIABLE:
	  ((finfo.vbr==MPG123_ABR)?MPS_VBR_MODE_AVERAGE:
	   MPS_VBR_MODE_UNKNOWN)));
      if (track_info->sampling_rate > 0) {
	total_samples = mpg123_length_64(m);
	track_info->time_length = total_samples/track_info->sampling_rate;
      }
    }
  }

  mpg123_close(m);
  mpg123_delete(m);
  mpg123_exit();

 ERR_OUT:
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);

  return rv;
}


/*
 * copy_genre_and_convert_if_necessary()
 *    Sometimes the genre string in a v2 string is encoded as "(<index>)"
 *    rather than a string representing the name of the genre.  We want to 
 *    convert to the name.
 *    
 */
static int copy_genre_and_convert_if_necessary(char *to, char *from)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int i;
  char numstring[3];

  MPS_DBG_PRINT("Entering %s(0x%08x, %s)\n", __func__, (unsigned int)to, from);
  memset (numstring, 0, 3);
  if (from[0] == '('){
    for (i=1; i<4; i++) {
      if (from[i] >= '0' && from[i] < '9') 
	numstring[i-1] = from[i];      
      else 
	break;
    }
    if (strlen(numstring) > 0)
      strcpy(to, genre_table[atoi(numstring)]);
    else 
      strcpy(to, "Unknown");
  }
  else {
    for (i=0; i<MPS_MAX_TRACK_INFO_FIELD_LENGTH; i++) {
      to[i]=from[i];
      if (from[i] == '\0')
	break;
    }
  }
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


int copy_string_without_edge_whitespace(char *to, char *from, int size)
{
  int rv = MPS_LIBRARYDBD_SUCCESS;
  int i, real_beginning;
  MPS_DBG_PRINT("Entering %s(0x%08x, %s, %d)\n", 
	    __func__, (unsigned int)to, from, size);

  for(i = 0, real_beginning = 0; i < size; i++) {
    if (from[i] == ' ' || from[i] == '\t')
      real_beginning++;
    else
      break;
  } 

  for(i =0; 
      i < (size - 1) && from[real_beginning] != '\0'; 
      i++, real_beginning++) {
    to[i] = from[real_beginning];
  }
  
  to[i] = '\0'; // Make sure there's a terminating null at end

  // remove trailing whitespace
  for (i=i-1; i >= 0; i--) {
    if (to[i] == ' ' || to[i] == '\t')
      to[i] = '\0';
    else
      break;
  }
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

/* update_artist_table() and update_album_table()
   add the artist/album to the respective table if necessary,
   an return the artist_album ID either way.  If these
   functions return a negative value, then an error has 
   occurred. 
*/

static long update_artist_table(char *artist, sqlite3 *db)
{
  long rv = 0, tempLong;
  char whereClause[MAX_SQL_STATEMENT_LENGTH];
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  char *zErrMsg = 0;
  int sqlite_rc;
  static char cachedArtistName[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  static long cachedArtistId;

  snprintf(whereClause, MAX_SQL_STATEMENT_LENGTH,
	   "artist='%s'", artist);
  
  if (!strncmp(artist, cachedArtistName, MPS_MAX_TRACK_INFO_FIELD_LENGTH)) {
    return cachedArtistId;
  }
 
  tempLong = mps_librarydb_get_long(db, "mps_library_artists", "artist_id",
				     whereClause, &rv);
  
  if (tempLong != MPS_LIBRARYDBD_SUCCESS) {
    if (tempLong == MPS_LIBRARYDBD_NO_RESULTS) {
      /* if it doesn't already exist, add it */
      snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	       "insert into mps_library_artists "
	       "values(%ld, '%s'); ", ++highestArtistId, artist);

      MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
      sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
      
      if( sqlite_rc != SQLITE_OK ){
	MPS_LOG("SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
	highestArtistId--;
	rv = MPS_LIBRARYDBD_FAILURE;
      }
      else {
	rv = highestArtistId;
	numberArtistsAdded++;
      }
    }
  }
  
  strncpy(cachedArtistName, artist, MPS_MAX_TRACK_INFO_FIELD_LENGTH);
  cachedArtistId = rv;
    
  return rv;
}

static long update_album_table(char *album, long artistId, sqlite3 *db)
{
  long rv = 0, tempLong;
  char whereClause[MAX_SQL_STATEMENT_LENGTH];
  char sqlStatement[MAX_SQL_STATEMENT_LENGTH];
  char *zErrMsg = 0;
  int sqlite_rc;
  static char cachedAlbumName[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  static long cachedAlbumId;
  static long cachedArtistId;

  snprintf(whereClause, MAX_SQL_STATEMENT_LENGTH,
	   "album='%s' AND artist_id=%ld", album, artistId);

  if (!strncmp(album, cachedAlbumName, MPS_MAX_TRACK_INFO_FIELD_LENGTH)
      && artistId == cachedArtistId) {
    return cachedAlbumId;
  }
   
  tempLong = mps_librarydb_get_long(db, "mps_library_albums", "album_id",
				     whereClause, &rv);
  
  if (tempLong != MPS_LIBRARYDBD_SUCCESS) {
    if (tempLong == MPS_LIBRARYDBD_NO_RESULTS) {
      /* if it doesn't already exist, add it */
      snprintf(sqlStatement, MAX_SQL_STATEMENT_LENGTH,
	       "insert into mps_library_albums "
	       "values(%ld, %ld, '%s'); ", ++highestAlbumId, artistId, album);

      MPS_DBG_PRINT("Calling sqlite3_exec on \"%s\"\n", sqlStatement);
      sqlite_rc = sqlite3_exec(db, sqlStatement, NULL, 0, &zErrMsg);
      
      if( sqlite_rc != SQLITE_OK ){
	MPS_LOG("SQL error: %s\n", zErrMsg);
	sqlite3_free(zErrMsg);
	highestAlbumId--;
	rv = MPS_LIBRARYDBD_FAILURE;
        goto OUT;
      }
      else {
	rv = highestAlbumId;
	numberAlbumsAdded++;
      }
    }
  } 

  strncpy(cachedAlbumName, album, MPS_MAX_TRACK_INFO_FIELD_LENGTH);
  cachedAlbumId = rv;
  cachedArtistId = artistId;

 OUT:  
  return rv;
}


