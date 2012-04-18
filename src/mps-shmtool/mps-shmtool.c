#include <stdio.h>
#include <stdlib.h>
#include "libmpsserver-private.h"

int opt;
mps_config_param_t shmfile_param;

static int print_shm_structure();
static void usage(char *command_name);


static void usage(char *command_name)
{
  printf("Usage:\n");
  printf("  %s [-h] [-i]\n", command_name);
  exit(1);
}

int main(int argc, char *argv[])
{
  bool initialize = false;

  while ((opt=getopt(argc, argv, "hi")) != -1) {
    switch(opt) {
    case 'h':
      usage(argv[0]);
      break;
    case 'i':
      initialize = true;
      break;
    default:
      printf("unknown argument '%c'\n", opt);
      usage(argv[0]);
      break;
    }      
  }
  
  strcpy(shmfile_param.name, "shm_file");
  mps_constants_get_param(&shmfile_param);

  if (initialize) {
    if (initialize_shared_memory_server()
	< MPS_SUCCESS) {
      printf("Error initializing shared memory region.\"%s\"\n",
	     shmfile_param.value);
      exit(1);
    }
  }
  else {
    if (mps_initialize_client()
	< MPS_SUCCESS) {
      printf("Error connecting to shared memory region \"%s\"\n",
	     shmfile_param.value);
      exit(1);
    }
  }
  
  print_shm_structure();
}


static int print_shm_structure()
{
  int rv = MPS_SUCCESS;
  mps_shm_region_t *p = mps_shm_region;

  printf("*mps_shm_region = {\n");
  printf("  control = {\n");
  printf("    opcode = %s\n", MPS_OPCODE_T_TO_STRING(p->control.opcode));
  printf("    skip_method = %s\n", MPS_SKIP_T_TO_STRING(p->control.skip_method));
  printf("    playlist_entry_id = %d\n", p->control.playlist_entry_id);
  printf("    stream_file_name = \"%s\"\n", p->control.stream_file_name);
  printf("  }\n");
  printf("  player_status = {\n");
  printf("    pid = %d\n", p->player_status.pid);
  printf("    state = %s\n", MPS_STATE_T_TO_STRING(p->player_status.state));  
  printf("    element_type = %s\n", 
	 MPS_LIBRARY_ELEMENT_TYPE_TO_STRING(p->player_status.element_type));
  printf("    current_playlist_entry_id = %ld\n", 
	 p->player_status.current_playlist_entry_id);
  printf("    previous_playlist_entry_id = %ld\n", 
	 p->player_status.previous_playlist_entry_id);
  printf("    current_stream_file_name = \"%s\"\n", 
	 p->player_status.current_stream_file_name);
  printf("  }\n");
  printf("  playlist_status = {\n");
  printf("    change_counter = %d\n", p->playlist_status.change_counter);
  printf("  }\n");
  printf("  library_status = {\n");
  printf("    change_counter = %d\n", p->library_status.change_counter);
  printf("  }\n");
  printf("}\n");
  
  return rv;
}
