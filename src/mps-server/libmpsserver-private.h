#define _XOPEN_SOURCE 700
#include "libmpsserver.h"

#define OPSIGNAL SIGUSR1
#define USING_UCLIBC

#define MPS_LIBRARY_SEPARATOR '/'
#define MPS_COMMAND_TIMEOUT_SEC 2
#define MPS_MAX_FILENAME_LENGTH 1024 
#define MPS_MAX_ELEMENTS_PER_LIST 51200
#define MAX_SQL_STATEMENT_LENGTH 4096

typedef enum {
  OPCODE_STOP=0,
  OPCODE_PLAY,
  OPCODE_PAUSE,
  OPCODE_GET_STATE,
  OPCODE_UNKNOWN
} mps_opcode_t;

#define MPS_OPCODE_T_TO_STRING(x) \
  ((x==OPCODE_STOP)?"OPCODE_STOP": \
  ((x==OPCODE_PLAY)?"OPCODE_PLAY": \
  ((x==OPCODE_PAUSE)?"OPCODE_PAUSE": \
  ((x==OPCODE_GET_STATE)?"OPCODE_GET_STATE": \
   "OPCODE_UNKNOWN"))))
  
typedef enum {
   SKIP_ABSOLUTE=0,
   SKIP_CURRENT,
   SKIP_NEXT,
   SKIP_PREVIOUS,
   SKIP_UNKNOWN
} mps_skip_t;

#define MPS_SKIP_T_TO_STRING(x) \
  ((x==SKIP_ABSOLUTE)?"SKIP_ABSOLUTE": \
  ((x==SKIP_CURRENT)?"SKIP_CURRENT": \
  ((x==SKIP_NEXT)?"SKIP_NEXT": \
  ((x==SKIP_PREVIOUS)?"SKIP_PREVIOUS": \
   "SKIP_UNKNOWN"))))

/* valid forms of control message:
 *
 * opcode: <OPCODE_STOP|OPCODE_PAUSE>
 *
 * opcode: OPCODE_PLAY
 * skip_method: <SKIP_NEXT|SKIP_PREVIOUS|SKIP_CURRENT>
 *
 * opcode: OPCODE_PLAY
 * skip_method: SKIP_ABSOLUTE
 * playlist_entry_id <id>
 *
 * opcode: OPCODE_GET_STATE
 */

typedef struct mps_control_t {
  mps_opcode_t opcode;
  mps_skip_t skip_method;
  int playlist_entry_id;
  char stream_file_name[MPS_MAX_FILENAME_LENGTH];
} mps_control_t;

typedef struct mps_shm_region_t {
  mps_control_t control;
  mps_player_status_t player_status;
  mps_playlist_status_t playlist_status;
  mps_library_status_t library_status;
} mps_shm_region_t;

extern mps_shm_region_t *mps_shm_region;

// private functions
extern int initialize_shared_memory_server();
extern int mps_library_get_file_path_from_id(long id, char *path);
extern int mps_configuration_get_filename(char *filename);
extern int mps_constants_get_filename(char *filename);
extern int mps_constants_set_param(mps_config_param_t *param);
extern int get_one_int64_value(sqlite3 *db, char *query, long *value);
extern int get_two_int64_values(sqlite3 *db, char *query, long *value1, long *value2);


/* shared memory functions not supported by older versions of uclibc,
   but they're very simple, so we'll include them if necessary */
#ifdef USING_UCLIBC
extern int shm_open (const char *, int, mode_t);
extern int shm_unlink (const char *);
#endif
