/*
 * mps-playlist.c -
 *   "mps-playlist" is a command line utility that is used to access the
 *    playlist functions in libmpsserver.
 *
 */     

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "mps-command-shared.h"

static void usage(char *);
static void get_and_print_playlist(int, int);
static void get_and_print_current();
static void get_and_print_next();
static void get_and_print_previous();
static void get_and_print_last();
static void get_and_print_count();
static void get_and_print_status();

static void string_array_to_long_array(int len, char *str_ary[], 
				       long **long_ary);

int main (int argc, char *argv[])
{
  int i;
  mps_player_status_t player_status;
  mps_library_element_type_t tmp_element_type;
  long *tmp_long_ptr;

  if (mps_command_init_and_getopts(argc, argv) 
      != MPS_SUCCESS) {
    usage(argv[0]);
  }

  if (mps_command_num_tokens < 1) {
    usage(argv[0]);
  }

  if (!strcmp(mps_command_tokens[0], "get")) {
    if (mps_command_num_tokens == 2) {
      if (!strcmp(mps_command_tokens[1], "all")) {
	get_and_print_playlist(MPS_PLAYLIST_BEGINNING, MPS_PLAYLIST_ALL);
      }
      else if (!strcmp(mps_command_tokens[1], "current")) {
	get_and_print_current();
      }
      else if (!strcmp(mps_command_tokens[1], "next")) {
	get_and_print_next();
      }
      else if(!strcmp(mps_command_tokens[1], "previous")) {
	get_and_print_previous();
      }
      else if(!strcmp(mps_command_tokens[1], "last")) {
	get_and_print_last();
      }
      else if(!strcmp(mps_command_tokens[1], "count")) {
	get_and_print_count();
      }
      else if(!strcmp(mps_command_tokens[1], "status")) {
	get_and_print_status();
      }
      else {
	usage(argv[0]);
      }
    }
    else if (mps_command_num_tokens == 3) {
      int start, count;

      if (!strcmp(mps_command_tokens[1], "current")) {
	mps_player_get_status(&player_status);
	start=player_status.current_playlist_entry_id;
      }
      else if (is_decimal_string(mps_command_tokens[1])) {
	start=atoi(mps_command_tokens[1]);
      }
      else {
	usage(argv[0]);
      }

      if (!strcmp(mps_command_tokens[2], "end")) {
	count=MPS_PLAYLIST_ALL; 
      }
      else if (is_decimal_string(mps_command_tokens[2])) {
	count=atoi(mps_command_tokens[2]);
      }
      else {
	usage(argv[0]);
      }

      get_and_print_playlist(start, count);
    }
    else {
      usage(argv[0]);
    }
  }
  else if(!strcmp(mps_command_tokens[0], "remove")) {
    if (mps_command_num_tokens == 2) {
      if(!strcmp(mps_command_tokens[1], "all")) {
	mps_player_stop();
	mps_playlist_remove(MPS_PLAYLIST_BEGINNING, MPS_PLAYLIST_ALL);
	mps_player_play_current();
      }
      else if (is_decimal_string(mps_command_tokens[1])) {
	mps_playlist_remove(atoi(mps_command_tokens[1]), 1);
      }
      else {
	usage(argv[0]);
      }
      mps_command_print_header();
    }
    else {
      usage(argv[0]);
    }
  }
  else if(!strcmp(mps_command_tokens[0], "add")) {
    if (mps_command_num_tokens >= 3) {
      string_array_to_long_array(mps_command_num_tokens-2,
				 &mps_command_tokens[2],
				 &tmp_long_ptr);

      if(!strcmp(mps_command_tokens[1], "end")) {
	mps_playlist_add_after(MPS_PLAYLIST_END, 
			       mps_command_num_tokens-2, 
			       tmp_long_ptr);
	free(tmp_long_ptr);
      }
      else if(!strcmp(mps_command_tokens[1], "beginning")) {
	mps_playlist_add_after(MPS_PLAYLIST_BEGINNING, 
			       mps_command_num_tokens-2, 
			       tmp_long_ptr);
	free(tmp_long_ptr);
      }
      else if (is_decimal_string(mps_command_tokens[1])) {
	mps_playlist_add_after(atoi(mps_command_tokens[1]), 
			       mps_command_num_tokens-2, 
			       tmp_long_ptr);
	free(tmp_long_ptr);
      }
      else {
	usage(argv[0]);
      }
      mps_command_print_header();
    }
    else {
      usage(argv[0]);
    }
  }
  else {
    usage(argv[0]);
  }
}

static void get_and_print_playlist(int start_id, int count) {
  mps_playlist_list_t *list;
  mps_playlist_entry_t *ple_ptr;
  int i;
  mxml_node_t *xml, *playlist, *entries, *entry;
  char buffer[OUTPUT_BUFFER_SIZE];
  mps_player_status_t status;
  mps_library_track_info_t track_info;

  MPS_DBG_PRINT("Entering %s(%d, %d)\n", __func__, start_id, count);

  buffer[0]='\0';

  mps_player_get_status(&status);

  mps_playlist_get(&list, start_id, count);
  
    
  xml=mxmlNewXML("1.0");
  
  playlist=mxmlNewElement(xml, "playlist");
  entries=mxmlNewElement(playlist, "entries");
  mxmlElementSetAttrf(entries, "count", "%d", 
                      list->number_of_entries);
  mxmlElementSetAttrf(entries, "current", "%ld", 
                      status.current_playlist_entry_id);
  for (ple_ptr = list->entry_head; 
       ple_ptr != NULL;
       ple_ptr = ple_ptr->next) {
    entry=mxmlNewElement(entries, "entry");
    mxmlElementSetAttrf(entry, "playlist_id", "%ld", ple_ptr->playlist_id);
    mxmlElementSetAttrf(entry, "title_id", "%ld", ple_ptr->title_id);
    mps_library_title_get_info(ple_ptr->title_id, &track_info);

#if 0
    mps_scrub_invalid_utf8(track_info.artist);
    mps_scrub_leading_and_invalid_spaces(track_info.artist);
#endif
    mxmlElementSetAttrf(entry, "artist", "%s",
#if 0
                        (!strcmp(track_info.artist, "?")) ?
                        "Unknown Artist" :
#endif 
                        track_info.artist);
#if 0
    mps_scrub_invalid_utf8(track_info.album);
    mps_scrub_leading_and_invalid_spaces(track_info.album); 
#endif
    mxmlElementSetAttrf(entry, "album", "%s", 
#if 0
                        (!strcmp(track_info.album, "?")) ?
                        "Unknown Album" :
#endif 
                        track_info.album);
#if 0
    mps_scrub_invalid_utf8(track_info.title);
    mps_scrub_leading_and_invalid_spaces(track_info.title);
#endif
    mxmlElementSetAttrf(entry, "name", "%s", 
#if 0
                        (!strcmp(track_info.title, "?")) ?
                        "Unknown Title" :
#endif 
                        track_info.title); 
  }
  mxmlSetWrapMargin(0);
  mxmlSaveString(xml, buffer, sizeof(buffer), 
                 mps_command_whitespace_callback);

  mps_playlist_free(&list);

  mps_command_print_header();
  printf("%s", buffer);    
}

static void get_and_print_current() {
  mps_player_status_t status;
  
  mps_player_get_status(&status);

  get_and_print_playlist(status.current_playlist_entry_id, 1);
}

static void get_and_print_next() {
  mps_player_status_t status;
  
  mps_player_get_status(&status);

  get_and_print_playlist
    (mps_playlist_get_next_id(status.current_playlist_entry_id), 1);
}

static void get_and_print_previous() {
  mps_player_status_t status;
  
  mps_player_get_status(&status);

  get_and_print_playlist
    (mps_playlist_get_previous_id(status.current_playlist_entry_id), 1);
}

static void get_and_print_last() {
  get_and_print_playlist(mps_playlist_get_last_id(), 1);
}

static void get_and_print_count() {
  int count;
  mxml_node_t *xml;
  mxml_node_t *entries;
  char buffer [1024];

  count=mps_playlist_get_count();

  xml=mxmlNewXML("1.0");
  entries=mxmlNewElement(xml, "entries");
  mxmlElementSetAttrf(entries, "count", "%d", count);
  mxmlSetWrapMargin(0);
  mxmlSaveString(xml, buffer, sizeof(buffer), 
                 mps_command_whitespace_callback);
  
  mps_command_print_header();
  printf("%s", buffer);    
}

void get_and_print_status()
{
  mps_playlist_status_t playlist_status;
  int status_code=MPS_SUCCESS;
  mxml_node_t *xml, *status, *param;

  status_code=mps_playlist_get_status(&playlist_status);

  if (status_code == MPS_SUCCESS) {
    char buffer[1024];
    buffer[0]='\0';

    xml=mxmlNewXML("1.0");
    status=mxmlNewElement(xml, "status");
    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "%s", 
                        "change_counter");
    mxmlElementSetAttrf(param, "value", "%d", 
                        playlist_status.change_counter);
    MPS_DBG_PRINT("Preparing xml output\n");
    mxmlSetWrapMargin(0);
    mxmlSaveString(xml, buffer, sizeof(buffer), 
                   mps_command_whitespace_callback);	  
    
    mps_command_print_header();
    printf("%s", buffer);
  }
  else {
    MPS_LOG("Error: mps_playlist_get_state() returned an error code.\n");
  }    
}

static void usage(char *command_name) {
  printf("Usage:\n");
  printf("  %s get count\n", command_name);
  printf("  %s get all\n", command_name);
  printf("  %s get current\n", command_name);
  printf("  %s get next\n", command_name);
  printf("  %s get previous\n",command_name);
  printf("  %s get status\n",command_name);
  printf("  %s get current|<start_playlist_id> end|<count>\n", 
	 command_name);
  printf("\n");
  printf("  %s remove all\n",command_name);
  printf("  %s remove <playlist_id>\n",command_name);
  printf("\n");
  printf("  %s add end <track_id ...>\n",
	 command_name);
  printf("  %s add end <album_id|file_path_id>\n",command_name);
  printf("  %s add beginning <track_id ...>\n",
	 command_name);
  printf("  %s add beginning <album_id|file_path_id>\n",command_name);
  printf("  %s add <playlist_id> <track_id ...>\n",
	 command_name);
  printf("  %s add <playlist_id> <album_id|file_path_id>\n",command_name);
  printf("\n");
  exit(1);
}

static void string_array_to_long_array(int len, char *str_ary[], long **long_ary)
{
  int i;

  *long_ary = malloc(len * sizeof(long));

  for(i = 0; i < len; i++) {
    (*long_ary)[i] = atol(str_ary[i]);
  }
}

