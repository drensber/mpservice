/*
 * mps-player.c -
 *   "mps-player" is a command line utility that is used to access the
 *   player control and monitoring functions in libmpsserver.
 *
 */     


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "mps-command-shared.h"

static void usage(char *);

int main (int argc, char *argv[])
{

  int i;
  mps_player_status_t player_status;
  mps_playlist_status_t playlist_status;
  mps_library_status_t library_status;
  int status_code=MPS_SUCCESS;

  if (mps_command_init_and_getopts(argc, argv) 
      != MPS_SUCCESS) {
    usage(argv[0]);
  }

  if (mps_command_num_tokens < 1) {
    usage(argv[0]);
  }

  if (!strcmp(mps_command_tokens[0], "stop")) {
    mps_player_stop();
    mps_command_print_header();
  }
  else if (!strcmp(mps_command_tokens[0], "pause")) {
    mps_player_pause();
    mps_command_print_header();
  }
  else if (!strcmp(mps_command_tokens[0], "play")) {
    if (!strcmp(mps_command_tokens[1], "current")) {
      mps_player_play_current();
    }
    else if (!strcmp(mps_command_tokens[1], "next")) {
      mps_player_play_next();
    }
    else if (!strcmp(mps_command_tokens[1], "previous")) {
      mps_player_play_previous();
    }
    else if (!strcmp(mps_command_tokens[1], "absolute")) {
      if (mps_command_num_tokens == 3) {
	if (is_decimal_string(mps_command_tokens[2])) {
	  mps_player_play_absolute(atoi(mps_command_tokens[2]));
	}
	else {
	  usage(argv[0]);
	}
      }
      else {
	usage(argv[0]);
      }
    }
    else {
      usage(argv[0]);
    }
    mps_command_print_header();
  }
  else if (!strcmp(mps_command_tokens[0], "get")) {
    if (mps_command_num_tokens < 2) {
      usage(argv[0]);
    }
    else if (!strcmp(mps_command_tokens[1], "summary")) {
      status_code=mps_playlist_get_status(&playlist_status);
      if (status_code != MPS_SUCCESS) {
	MPS_LOG("Error: mps_playlist_get_status() returned an error code.\n");
      }
      else {
	status_code=mps_library_get_status(&library_status);
	if (status_code != MPS_SUCCESS) {
	  MPS_LOG("Error: mps_library_get_status() returned an error code.\n");
	}
	else {
	  status_code=mps_player_get_status(&player_status);
	  if (status_code != MPS_SUCCESS) {
	    MPS_LOG("Error: mps_player_get_status() returned an error code.\n");
	  }
	}
      }

      if (status_code == MPS_SUCCESS) {
	char buffer[OUTPUT_BUFFER_SIZE];
	char *buffer_pointer=&buffer[0];
	int num_chars=0;
	int buffer_size=sizeof(buffer);
	mxml_node_t *xml, *status, *param, *state, *element_type, *current_playlist_entry_id,
	  *previous_playlist_entry_id, *current_stream_file_name, *playlist_change_counter,
	  *library_change_counter;

	buffer[0]='\0';

        xml=mxmlNewXML("1.0");
        status=mxmlNewElement(xml, "summary");

	if (mps_command_num_tokens == 2) {
          MPS_DBG_PRINT("Assembling XML message with all status\n");
          param=mxmlNewElement(status, "param");
          mxmlElementSetAttrf(param, "name", "state");
          mxmlElementSetAttrf(param, "value", "%s", 
                              MPS_STATE_T_TO_STRING(player_status.state));

          param=mxmlNewElement(status, "param");
          mxmlElementSetAttrf(param, "name", "current_playlist_entry_id");
          mxmlElementSetAttrf(param, "value", "%ld", 
                              player_status.current_playlist_entry_id);

          param=mxmlNewElement(status, "param");
          mxmlElementSetAttrf(param, "name", "library_change_counter");
          mxmlElementSetAttrf(param, "value", "%d", 
                              library_status.change_counter);

          param=mxmlNewElement(status, "param");
          mxmlElementSetAttrf(param, "name", "librarydbd_state");
          mxmlElementSetAttrf(param, "value", "%s", 
             MPS_LIBRARYDB_STATE_T_TO_STRING(library_status.librarydbd_state));

          param=mxmlNewElement(status, "param");
          mxmlElementSetAttrf(param, "name", "playlist_change_counter");
          mxmlElementSetAttrf(param, "value", "%d", 
                              playlist_status.change_counter);

          MPS_DBG_PRINT("Finished assembling XML message with summary\n");
	  
	}
	else {
	  usage(argv[0]);
	}

        MPS_DBG_PRINT("Preparing xml output\n");
        mxmlSetWrapMargin(0);
        mxmlSaveString(xml, buffer, sizeof(buffer), 
                       mps_command_whitespace_callback);	  
        
	mps_command_print_header();
	printf("%s", buffer);    
      }
    }
    else {
      usage(argv[0]);
    }
  }
  else {
    usage(argv[0]);
  }
  exit(status_code);
}

static void usage(char *command_name) 
{
  printf("Usage:\n");
  printf("  %s stop\n", command_name);
  printf("  %s pause\n", command_name);
  printf("\n");
  printf("  %s play <current|next|previous>\n",command_name);
  printf("  %s play absolute <entry id>\n", command_name);
  printf("\n");
  printf("  %s get summary\n", command_name);
  printf("\n");
  exit(1);
}


