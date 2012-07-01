/*
 * Name: libmpsserver.h
 *
 * Purpose: This file contains shared prototypes and definitions
 *          related to the libmpsserver server and clients.
 *
 */

#ifndef MPS_SERVER_LIB_H
#define MPS_SERVER_LIB_H

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <errno.h>
#include <sqlite3.h>
#include <mxml.h>

#define MPS_SUCCESS 0
#define MPS_FAILURE -1

#define MPS_MAX_LIBRARY_PATH_LENGTH 1024
#define MPS_MAX_FOLDERNAME_LENGTH 1024
#define MPS_MAX_TRACK_INFO_FIELD_LENGTH 512
#define MPS_EMPTY_STRING ""

/* title ID's are in the range 0-100M, artist ID's 100M-200M,
   and album ID's 200M-300M.  */
#define MPS_NULL_OBJECT_ID     0L
#define MPS_MIN_TITLE_ID       MPS_NULL_OBJECT_ID
#define MPS_MIN_ARTIST_ID      1000000L
#define MPS_MIN_ALBUM_ID       2000000L
#define MPS_MIN_FILE_PATH_ID   3000000L
#define MPS_MAX_TITLE_ID       MPS_MIN_ARTIST_ID-1
#define MPS_MAX_ARTIST_ID      MPS_MIN_ALBUM_ID-1
#define MPS_MAX_ALBUM_ID       MPS_MIN_FILE_PATH_ID-1
#define MPS_MAX_FILE_PATH_ID   0x4fffffff
#define MPS_TITLE_ID_ROOT      MPS_MIN_TITLE_ID
#define MPS_ARTIST_ID_ROOT     MPS_MIN_ARTIST_ID
#define MPS_ALBUM_ID_ROOT      MPS_MIN_ALBUM_ID
#define MPS_FILE_PATH_ID_ROOT  MPS_MIN_FILE_PATH_ID

#define MPS_IS_TITLE_ID(x) (x>=MPS_MIN_TITLE_ID&&x<=MPS_MAX_TITLE_ID)
#define MPS_IS_ARTIST_ID(x) (x>=MPS_MIN_ARTIST_ID&&x<=MPS_MAX_ARTIST_ID)
#define MPS_IS_ALBUM_ID(x) (x>=MPS_MIN_ALBUM_ID&&x<=MPS_MAX_ALBUM_ID)
#define MPS_IS_FILE_PATH_ID(x) (x>=MPS_MIN_FILE_PATH_ID&&x<=MPS_MAX_FILE_PATH_ID)

#define MPS_MAX_CONFIG_PARAM_LENGTH 512

/* 
 * the enums 
 */

typedef enum {
  MPS_OBJECT_TYPE_TITLE=0,
  MPS_OBJECT_TYPE_ARTIST,
  MPS_OBJECT_TYPE_ALBUM,
  MPS_OBJECT_TYPE_FILE_PATH,
  MPS__OBJECT_TYPE_UNKNOWN
} mps_database_object_type_t;

typedef enum {
  STATE_STOPPED=0,
  STATE_PAUSED,
  STATE_PLAYING,
  STATE_UNKNOWN
} mps_state_t;

#define MPS_STATE_T_TO_STRING(x) \
  ((x==STATE_STOPPED)?"STATE_STOPPED": \
  ((x==STATE_PAUSED)?"STATE_PAUSED": \
  ((x==STATE_PLAYING)?"STATE_PLAYING": \
   "STATE_UNKNOWN")))
  
// special codes for playlist entry ids
typedef enum {
  MPS_PLAYLIST_EMPTY=-100,
  MPS_PLAYLIST_BEGINNING=-101,
  MPS_PLAYLIST_END=-102,
  MPS_PLAYLIST_ALL=-103,
  MPS_PLAYLIST_UNKNOWN
} mps_playlist_entry_code_t;

#define MPS_PLAYLIST_ENTRY_CODE_TO_STRING(x) \
  ((x==MPS_PLAYLIST_EMPTY)?"MPS_PLAYLIST_EMPTY": \
  ((x==MPS_PLAYLIST_BEGINNING)?"MPS_PLAYLIST_BEGINNING": \
  ((x==MPS_PLAYLIST_END)?"MPS_PLAYLIST_END": \
  ((x==MPS_PLAYLIST_ALL)?"MPS_PLAYLIST_ALL": \
   "MPS_PLAYLIST_UNKNOWN"))))

typedef enum {
  MPS_LIBRARY_FOLDER=0,
  MPS_LIBRARY_MP3_FILE,
  MPS_LIBRARY_FLAC_FILE,
  MPS_LIBRARY_OGG_FILE,
  MPS_LIBRARY_AAC_FILE,
  MPS_LIBRARY_APPLE_LOSSLESS_FILE,
  MPS_LIBRARY_WMA_FILE,
  MPS_LIBRARY_AIF_FILE,
  MPS_LIBRARY_WAV_FILE,
  MPS_LIBRARY_MP3_STREAM,
  MPS_LIBRARY_UNKNOWN
} mps_library_element_type_t;

#define MPS_LIBRARY_ELEMENT_TYPE_TO_STRING(x) \
  ((x==MPS_LIBRARY_FOLDER)?"MPS_LIBRARY_FOLDER": \
  ((x==MPS_LIBRARY_MP3_FILE)?"MPS_LIBRARY_MP3_FILE": \
  ((x==MPS_LIBRARY_FLAC_FILE)?"MPS_LIBRARY_FLAC_FILE": \
  ((x==MPS_LIBRARY_OGG_FILE)?"MPS_LIBRARY_OGG_FILE": \
  ((x==MPS_LIBRARY_AAC_FILE)?"MPS_LIBRARY_AAC_FILE": \
  ((x==MPS_LIBRARY_APPLE_LOSSLESS_FILE)?"MPS_LIBRARY_APPLE_LOSSLESS_FILE": \
  ((x==MPS_LIBRARY_WMA_FILE)?"MPS_LIBRARY_WMA_FILE": \
  ((x==MPS_LIBRARY_AIF_FILE)?"MPS_LIBRARY_AIF_FILE": \
  ((x==MPS_LIBRARY_WAV_FILE)?"MPS_LIBRARY_WAV_FILE": \
  ((x==MPS_LIBRARY_MP3_STREAM)?"MPS_LIBRARY_MP3_STREAM":       \
   "MPS_LIBRARY_UNKNOWN"))))))))))

typedef enum {
  MPS_CHANNEL_MODE_UNKNOWN=0,
  MPS_CHANNEL_MODE_MONO,
  MPS_CHANNEL_MODE_STEREO,
  MPS_CHANNEL_MODE_JOINT_STEREO,
  MPS_CHANNEL_MODE_DUAL_CHANNEL
} mps_channel_mode_t;

#define MPS_CHANNEL_MODE_T_TO_STRING(x) \
  ((x==MPS_CHANNEL_MODE_MONO)?"MPS_CHANNEL_MODE_MONO": \
  ((x==MPS_CHANNEL_MODE_STEREO)?"MPS_CHANNEL_MODE_STEREO": \
  ((x==MPS_CHANNEL_MODE_JOINT_STEREO)?"MPS_CHANNEL_MODE_JOINT_STEREO": \
  ((x==MPS_CHANNEL_MODE_DUAL_CHANNEL)?"MPS_CHANNEL_MODE_DUAL_CHANNEL": \
   "MPS_CHANNEL_MODE_UNKNOWN"))))

typedef enum {
  MPS_VBR_MODE_UNKNOWN=0,
  MPS_VBR_MODE_CONSTANT,
  MPS_VBR_MODE_VARIABLE,
  MPS_VBR_MODE_AVERAGE
} mps_vbr_mode_t;

#define MPS_VBR_MODE_T_TO_STRING(x) \
  ((x==MPS_VBR_MODE_CONSTANT)?"MPS_VBR_MODE_CONSTANT": \
  ((x==MPS_VBR_MODE_VARIABLE)?"MPS_VBR_MODE_VARIABLE": \
  ((x==MPS_VBR_MODE_AVERAGE)?"MPS_VBR_MODE_AVERAGE": \
   "MPS_VBR_MODE_UNKNOWN")))

// Enum for the 5 states
typedef enum {
  LIBRARYDBD_STATE_INIT=0,
  LIBRARYDBD_STATE_CREATINGDB,
  LIBRARYDBD_STATE_UPDATINGDB,
  LIBRARYDBD_STATE_WAITING,
  LIBRARYDBD_STATE_SETTLING,
  LIBRARYDBD_STATE_UNKNOWN
} mps_librarydbd_state_t;

#define MPS_LIBRARYDB_STATE_T_TO_STRING(x) \
  ((x==LIBRARYDBD_STATE_INIT)?"LIBRARYDBD_STATE_INIT": \
  ((x==LIBRARYDBD_STATE_CREATINGDB)?"LIBRARYDBD_STATE_CREATINGDB": \
  ((x==LIBRARYDBD_STATE_UPDATINGDB)?"LIBRARYDBD_STATE_UPDATINGDB": \
  ((x==LIBRARYDBD_STATE_WAITING)?"LIBRARYDBD_STATE_WAITING": \
  ((x==LIBRARYDBD_STATE_SETTLING)?"LIBRARYDBD_STATE_SETTLING": \
   "LIBRARYDBD_STATE_UNKNOWN")))))

#define MPS_LIBRARY_ELEMENT_IS_MUSIC_FILE(x) \
  ((x.type)==MPS_LIBRARY_MP3_FILE)

/*
 * the public structs
 */
typedef struct mps_player_status_t {
  pid_t pid;
  mps_state_t state;
  mps_library_element_type_t element_type;
  long current_playlist_entry_id;
  long previous_playlist_entry_id;
  char current_stream_file_name[MPS_MAX_LIBRARY_PATH_LENGTH];
} mps_player_status_t;

typedef struct mps_playlist_status_t {
  int change_counter;
} mps_playlist_status_t;

typedef struct mps_library_status_t {
  int change_counter;
  mps_librarydbd_state_t librarydbd_state;
} mps_library_status_t;

typedef struct mps_playlist_entry_t {
  long playlist_id; 
  long title_id;
  struct mps_playlist_entry_t *next;  
} mps_playlist_entry_t;

typedef struct mps_playlist_list_t  {
  int number_of_entries;
  mps_playlist_entry_t *entry_head;
} mps_playlist_list_t;

typedef struct mps_library_element_t {
  long id;
  char *name;
} mps_library_element_t;

typedef struct mps_library_folder_t {
  int number_of_elements;
  long id;
  long parent;
  char name[MPS_MAX_FOLDERNAME_LENGTH];
  mps_library_element_t *elements;
} mps_library_folder_t;

typedef struct mps_library_track_info_t {
  long title_id;
  long file_length;
  long file_last_modification;
  long parent_path_id;
  char file_name[MPS_MAX_LIBRARY_PATH_LENGTH];
  mps_library_element_type_t file_type;
  char title[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  char album[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  long album_id;
  char artist[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  long artist_id;
  int year;
  char genre[MPS_MAX_TRACK_INFO_FIELD_LENGTH];
  int time_length;
  int play_count;
  long last_played;
  int ordinal_track_number;
  int bitrate;
  int sampling_rate;
  mps_channel_mode_t channel_mode;
  mps_vbr_mode_t vbr_mode;
} mps_library_track_info_t;

typedef struct mps_config_param_t {
  char name[MPS_MAX_CONFIG_PARAM_LENGTH];
  char value[MPS_MAX_CONFIG_PARAM_LENGTH];
  char type[MPS_MAX_CONFIG_PARAM_LENGTH];
} mps_config_param_t;

/*
 * Public function prototypes
 */

/* library related */
extern int mps_initialize_client();

/* config related */
extern int mps_configuration_get_param(mps_config_param_t *param);
extern int mps_configuration_set_param(mps_config_param_t *param);
extern int mps_constants_get_param(mps_config_param_t *param);
extern int mps_configuration_get_params(mps_config_param_t *params, int count);
extern int mps_constants_get_params(mps_config_param_t *params, int count);
extern int mps_configuration_set_params(mps_config_param_t *params, int count);

/* player related */
extern int mps_player_stop();
extern int mps_player_pause();
extern int mps_player_play_current();
extern int mps_player_play_next();
extern int mps_player_play_previous();
extern int mps_player_play_absolute(int);
extern int mps_player_get_status(mps_player_status_t *);

/* playlist related */
extern int  mps_playlist_get_count();
extern long mps_playlist_get_next_id(long entry_id);
extern long mps_playlist_get_previous_id(long entry_id);
extern long mps_playlist_get_last_id();
extern long mps_playlist_get_title_id_from_entry_id(long entry_id);
extern int  mps_playlist_add_after(long first_id, int num_entries, long *id_list);
extern int mps_playlist_remove(long first_id, int count);
extern int mps_playlist_get(mps_playlist_list_t **list, long first_id, int count);
extern int mps_playlist_get_status(mps_playlist_status_t *);
extern int mps_playlist_free(mps_playlist_list_t **);

/* library related */
extern long mps_library_get_artist_count();
extern long mps_library_get_album_count();
extern long mps_library_get_title_count();
extern int mps_library_hierarchy_get_tag_root(mps_library_folder_t **folder);
extern int mps_library_hierarchy_get_file_path_root(mps_library_folder_t **folder);
extern int mps_library_hierarchy_get(mps_library_folder_t **folder, long id);
extern int mps_library_artist_list_get(mps_library_folder_t **list);
extern int mps_library_album_list_get(mps_library_folder_t **list, int artist_id);
extern int mps_library_title_list_get(mps_library_folder_t **list, int album_id);
extern int mps_library_title_get_info(int id, mps_library_track_info_t *info);
extern int mps_library_random(mps_library_folder_t **list,
                              mps_database_object_type_t object_type, 
                              int count);
extern int mps_library_simple_search(mps_library_folder_t **list, char *searchstring, 
                                     mps_database_object_type_t object_type, int limit);
extern int mps_library_get_status(mps_library_status_t *);
extern int mps_library_folder_free(mps_library_folder_t *);

// Unimplemented
extern int mps_library_album_list_random(mps_library_folder_t **list, int count);
extern int mps_library_artist_list_random(mps_library_folder_t **list, int count);
extern int mps_library_sql_search(mps_library_folder_t **list, char *where_clause);
// end unimplemented


/* utility */
extern bool is_decimal_string(char *);

/* global variables */
//extern sem_t *mps_mutex;

#define DEBUG_TO_CONSOLE 0
#define DEBUG_TO_LOG 1

#ifdef DEBUG_ENABLED

extern int mps_debug_enabled;
extern int mps_debug_sink;

#define MPS_DBG_PRINT(args ...) { \
    char buff1[512], buff2[512];	  \
    if (mps_debug_enabled) {	  \
      sprintf(buff1, "%s(%d): ", __FILE__, __LINE__);	  \
      sprintf(buff2, args);		  \
      if (mps_debug_sink == DEBUG_TO_LOG) { \
	syslog(LOG_INFO, "%s%s", buff1, buff2);	\
      }	 else {					\
        fprintf(stderr, "%s%s", buff1, buff2);	\
      } } }                                            


#define MPS_LOG(args ...) { \
  if (mps_debug_enabled && (mps_debug_sink == DEBUG_TO_CONSOLE)) {   \
    fprintf(stderr, args);				      \
    syslog(LOG_INFO, args); } else {			      \
    syslog(LOG_INFO, args); } }

#else // DEBUG_ENABLED
#define MPS_DBG_PRINT(args ...)
#define MPS_LOG(args ...) syslog(LOG_INFO, args)
#endif


#endif /* MPS_SERVER_LIB_H */
