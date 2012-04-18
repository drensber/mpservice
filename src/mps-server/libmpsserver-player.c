#include "libmpsserver-private.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static int generic_command(mps_opcode_t, mps_state_t);
static int generic_play_command(mps_skip_t, int);
static int msleep(unsigned long msec);

int mps_debug_enabled;
int mps_debug_sink;

//sem_t *mps_mutex;

int mps_player_stop() 
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  rv =generic_command(OPCODE_STOP, STATE_STOPPED);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_player_pause() 
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  rv=generic_command(OPCODE_PAUSE, STATE_PAUSED);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}


int mps_player_play_current()
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  rv=generic_play_command(SKIP_CURRENT, 0);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_player_play_next()
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  rv=generic_play_command(SKIP_NEXT, 0);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_player_play_previous()
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  rv=generic_play_command(SKIP_PREVIOUS, 0);
  return rv;
}

int mps_player_play_absolute(int entry_id)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(%d)\n", __func__, entry_id);
  rv=generic_play_command(SKIP_ABSOLUTE, entry_id);
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_player_get_status(mps_player_status_t *status)
{
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s()\n", __func__);
  memcpy(status, &mps_shm_region->player_status, sizeof(mps_player_status_t));
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
  return rv;
}

int initialize_shared_memory_server() 
{
  int shmfd;
  int shared_seg_size = sizeof(mps_shm_region_t);
  mps_config_param_t shmfile_param;

  strcpy(shmfile_param.name, "shm_file");
  mps_constants_get_param(&shmfile_param);

  shm_unlink(shmfile_param.value);

  /* creating the shared memory object    --  shm_open()  */
  shmfd = shm_open(shmfile_param.value, 
		   O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);

  if (shmfd < 0) {
    perror("In shm_open()");
    MPS_LOG("Error calling shm_open().  errno=%s\n", strerror(errno));
    return MPS_FAILURE;
  }
  MPS_DBG_PRINT("Created shared memory object %s\n", shmfile_param.value);
  
#if 0 // no support in uClibc for named semaphores yet
  mps_mutex = sem_open(MPS_SEMAPHORE_NAME, O_CREAT, 0644, 1);
  if (mps_mutex == SEM_FAILED) {
    perror("In sem_open()");
    MPS_LOG("Error calling sem_open(). errno=%s\n", strerror(errno));
    exit(1);
  }
  MPS_DBG_PRINT("Created named sempahore %s\n", MPS_SEMAPHORE_NAME);
#endif

  ftruncate(shmfd, shared_seg_size);

  /* requesting the shared segment    --  mmap() */    
  mps_shm_region = 
    (struct mps_shm_region_t *)mmap(NULL, shared_seg_size, 
				    PROT_READ | PROT_WRITE, MAP_SHARED, 
				    shmfd, 0);
  if (mps_shm_region == NULL) {
    perror("In mmap()");
    MPS_LOG("Error calling mmap()\n");
    return MPS_FAILURE;
  }
  MPS_DBG_PRINT("Shared memory segment allocated correctly (%d bytes).\n", 
		shared_seg_size);
  
  memset(mps_shm_region, 0, sizeof(mps_shm_region_t));

  mps_shm_region->player_status.state=STATE_PLAYING;
  mps_shm_region->player_status.current_playlist_entry_id=MPS_PLAYLIST_EMPTY;
  mps_shm_region->player_status.previous_playlist_entry_id=MPS_PLAYLIST_EMPTY;
  mps_shm_region->player_status.pid=getpid();
	 
  return MPS_SUCCESS;
}

int mps_initialize_client()
{
  int shmfd, i;
  int shared_seg_size = sizeof(mps_shm_region_t);
  mps_config_param_t shmfile_param;

  /* attaching to the shared memory object    --  shm_open()  */
  strcpy(shmfile_param.name, "shm_file");
  mps_constants_get_param(&shmfile_param);

  /* try every 50ms for 2 seconds... if still haven't succeded after
     2 seconds, then give up and return error... servers and clients
     should be launched at the same time, so 2sec should be more than
     enough time to synchronize.
  */
  i=40;
  while ((shmfd = shm_open(shmfile_param.value, O_RDWR, S_IRWXU | S_IRWXG))
         == -1) {
    msleep(50);
    i--;
    if (i == 0) break;
  }

  if (shmfd < 0) {
    perror("In shm_open()");
    MPS_LOG("Error calling shm_open()\n");
    return MPS_FAILURE;
  }
  MPS_DBG_PRINT("Attached to shared memory object %s\n", shmfile_param.name);


#if 0 // no uClibc named semaphores yet
  mps_mutex = sem_open(MPS_SEMAPHORE_NAME, 0, 0644, 0);
  if (mps_mutex == SEM_FAILED) {
    perror("In sem_open()");
    MPS_LOG("Error calling sem_open()\n");
    return MPS_FAILURE;
  }
  MPS_DBG_PRINT("Attached to named sempahore %s\n", MPS_SEMAPHORE_NAME);
#endif

  /* requesting the shared segment    --  mmap() */    
  mps_shm_region = 
    (struct mps_shm_region_t *)mmap(NULL, shared_seg_size, 
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    shmfd, 0);
  if (mps_shm_region == NULL) {
    perror("In mmap()");
    MPS_LOG("Error calling mmap()\n");
    return MPS_FAILURE;
  }
  MPS_DBG_PRINT("Shared memory segment allocated correctly (%d bytes).\n", 
		 shared_seg_size);
  
  return MPS_SUCCESS;
}

bool is_decimal_string(char *string)
{
  int i;
  bool rv = true;
  for (i=0; string[i] != '\0'; i++) {
    if (!isdigit(string[i])) {
      rv=false;
      break;
    }
  }
  return rv;
}

 
static int generic_command(mps_opcode_t opcode, mps_state_t expected_state) 
{
#if 0 // no uClibc named semaphores yet
  int s; 
#endif
  int rv=MPS_SUCCESS;
  struct timespec now, timeout;

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  clock_gettime(CLOCK_REALTIME, &now);
  timeout.tv_sec = now.tv_sec + MPS_COMMAND_TIMEOUT_SEC;
  timeout.tv_nsec = now.tv_nsec;

#if 0 // no named semaphores
  if ((s=sem_timedwait(mps_mutex, &timeout)) != 0) { 
#endif
    MPS_DBG_PRINT("setting opcode\n");
    mps_shm_region->control.opcode = opcode;
    MPS_DBG_PRINT("sending signal to player\n");
    kill(mps_shm_region->player_status.pid, OPSIGNAL);
    
    MPS_DBG_PRINT("waiting for response from player\n");
    while (mps_shm_region->player_status.state != expected_state) {
      clock_gettime(CLOCK_REALTIME, &now);
      if ((now.tv_sec >= timeout.tv_sec) && (now.tv_nsec >= timeout.tv_nsec)) {
	MPS_LOG("Timed out waiting for status.state==\"%s\"\n", 
		 MPS_STATE_T_TO_STRING(expected_state));
	break;
      } 
      msleep(10);
    }

#if 0 // no named sempahores    
    sem_post(mps_mutex);
  }
  else {
    MPS_LOG("sem_timedout returned non-zero value %d, errno is %d\n", s, errno);
    rv=MPS_FAILURE;
  }
#endif

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int generic_play_command(mps_skip_t skip_method,
				int absolute_id) 
{
  int starting_id, expected_id;
  int rv=MPS_SUCCESS;
  MPS_DBG_PRINT("Entering %s(%s, %d)\n", __func__, 
		MPS_SKIP_T_TO_STRING(skip_method), absolute_id);
  starting_id=mps_shm_region->player_status.current_playlist_entry_id;

  MPS_DBG_PRINT("setting opcode(%d) and skip_method(%d)\n",
		OPCODE_PLAY, skip_method);
  mps_shm_region->control.opcode = OPCODE_PLAY;
  mps_shm_region->control.skip_method = skip_method;

  if (skip_method == SKIP_ABSOLUTE) {
    MPS_DBG_PRINT("setting playlist_entry_id (%d)\n", absolute_id);
    mps_shm_region->control.playlist_entry_id=absolute_id;
  }

  MPS_DBG_PRINT("sending signal to player\n");

  kill(mps_shm_region->player_status.pid, OPSIGNAL);

  switch(skip_method)
    {
    case SKIP_ABSOLUTE:
      expected_id=absolute_id;
      break;
    case SKIP_CURRENT:
      expected_id=starting_id;
      break;
    case SKIP_NEXT:
      expected_id=mps_playlist_get_next_id(starting_id);
      break;
    case SKIP_PREVIOUS:
      expected_id=mps_playlist_get_previous_id(starting_id);
      if (expected_id==MPS_PLAYLIST_BEGINNING) {
	expected_id=starting_id;
      }
      break;
    default:
      break;      
    }
  
  MPS_DBG_PRINT("expecting player_status.state to become %d\n", STATE_PLAYING);
  while (mps_shm_region->player_status.state != STATE_PLAYING) {
      msleep(10);
  }
  MPS_DBG_PRINT("expected player_status.state change occurred.\n");

  MPS_DBG_PRINT("expecting current_playlist_id to become %d\n", expected_id);
  while (1) {
    if (mps_shm_region->player_status.current_playlist_entry_id 
	== expected_id) {
      MPS_DBG_PRINT("expected current_playlist_entry_id change occurred.\n");
      break;
    }
    msleep(10);
  }
  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int msleep(unsigned long milisec)
{
    struct timespec req={0};
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
         continue;
    return 1;
}

#ifdef USING_UCLIBC
/* shm_open - open a shared memory file */

/* Copyright 2002, Red Hat Inc. */

#include <string.h>
#include <limits.h>

int
shm_open (const char *name, int oflag, mode_t mode)
{
  int fd;
  char shm_name[PATH_MAX+20] = "/dev/shm/";

  /* skip opening slash */
  if (*name == '/') {
    ++name;
  }

  /* create special shared memory file name and leave enough space to
     cause a path/name error if name is too long */
  strncpy (shm_name + 9, name, PATH_MAX + 10);
  
  fd = open (shm_name, oflag, mode);

  if (fd != -1) {
    /* once open we must add FD_CLOEXEC flag to file descriptor */
    int flags = fcntl (fd, F_GETFD, 0);
    
    if (flags >= 0) {
      flags |= FD_CLOEXEC;
      flags = fcntl (fd, F_SETFD, flags);
    }

    /* on failure, just close file and give up */
    if (flags == -1) {
      close (fd);
      fd = -1;
    }
  }
  
  return fd;
}

int shm_unlink (const char *name)
{
  int rc;
  char shm_name[PATH_MAX+20] = "/dev/shm/";

  /* skip opening slash */
  if (*name == '/') {
    ++name;
  }

  /* create special shared memory file name and leave enough space to
     cause a path/name error if name is too long */
  strncpy (shm_name + 9, name, PATH_MAX + 10);

  rc = unlink (shm_name);

  return rc;
}
#endif

