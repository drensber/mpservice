/*
 * mps-library.c -
 *   "mps-library" is a command line utility that is used to access the
 *    library functions in libmpsserver.
 *
 */     

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "mps-command-shared.h"

static void usage(char *);
static void get_and_print_root();
static void get_and_print_path(long);
static void get_and_print_artists();
static void get_and_print_albums(int);
static void get_and_print_titles(int);
static void get_and_print_info(int);
static void get_and_print_status();
static void get_and_print_random(int);
static void get_and_print_search_results(char *search_string, int count);
static void print_folder(mps_library_folder_t *);
static void print_folders(mps_library_folder_t **, int count);

int main(int argc, char *argv[])
{

  int i;
  mps_player_status_t player_status;

  if (mps_command_init_and_getopts(argc, argv) 
      != MPS_SUCCESS) {
    usage(argv[0]);
  }

  if (mps_command_num_tokens < 1) {
    usage(argv[0]);
  }

  if (!strcmp(mps_command_tokens[0], "get")) {
    if (mps_command_num_tokens == 2) {
      if (!strcmp(mps_command_tokens[1], "root")) {
	get_and_print_root();
      }
      else if (!strcmp(mps_command_tokens[1], "artists")) {
	get_and_print_artists();
      }
      else if (!strcmp(mps_command_tokens[1], "status")) {
	get_and_print_status();
      }
      else {
	usage(argv[0]);
      }
    }
    else if (mps_command_num_tokens == 3) {
      if (!strcmp(mps_command_tokens[1], "path")) {
	get_and_print_path(atol(mps_command_tokens[2]));
      }
      else if (!strcmp(mps_command_tokens[1], "albums")) {
	get_and_print_albums(atoi(mps_command_tokens[2]));
      }
      else if (!strcmp(mps_command_tokens[1], "titles")) {
	get_and_print_titles(atoi(mps_command_tokens[2]));
      }
      else if (!strcmp(mps_command_tokens[1], "info")) {
	get_and_print_info(atoi(mps_command_tokens[2]));
      }
      else if (!strcmp(mps_command_tokens[1], "random")) {
	get_and_print_random(atoi(mps_command_tokens[2]));
      }
    }
    else if (mps_command_num_tokens == 4) {
      if (!strcmp(mps_command_tokens[1], "search")) {
        get_and_print_search_results(mps_command_tokens[2],
                                     atoi(mps_command_tokens[3]));
      }
      else {
	usage(argv[0]);
      }
    }
    else {
      usage(argv[0]);
    }
  }
}


static void usage(char *command_name) {
  printf("Usage:\n");
  printf("  %s get root\n", command_name);
  printf("  %s get status\n", command_name);
  printf("  %s get path <id>\n", command_name);
  printf("  %s get artists\n", command_name);
  printf("  %s get albums <artist id>\n", command_name);
  printf("  %s get titles <album id>\n", command_name);
  printf("  %s get info <title id>\n", command_name);
  printf("  %s get random <count>\n", command_name);
  printf("  %s get search \"<phrase>\" <count>\n", command_name);
  printf("\n");
  exit(1);
}

static void get_and_print_root() 
{
  get_and_print_path(MPS_FILE_PATH_ID_ROOT);
}

static void get_and_print_artists()
{
  int rv;
  mps_library_folder_t *folder;

  MPS_DBG_PRINT("Entering %s()\n", __func__);
  
  rv = mps_library_artist_list_get(&folder);

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_artist_list_get()..\n");
  }
  else {
    print_folder(folder);
  }

  mps_library_folder_free(folder);
  
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

static void get_and_print_albums(int artist_id)
{
  int rv;
  mps_library_folder_t *folder;

  MPS_DBG_PRINT("Entering %s(%d)\n", __func__, artist_id);
  
  rv = mps_library_album_list_get(&folder, artist_id);

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_album_list_get(%d)..\n", artist_id);
  }
  else {
    print_folder(folder);
  }

  mps_library_folder_free(folder);
  
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

static void get_and_print_titles(int album_id)
{
  int rv;
  mps_library_folder_t *folder;

  MPS_DBG_PRINT("Entering %s(%d)\n", __func__, album_id);
  
  rv = mps_library_title_list_get(&folder, album_id);

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_title_list_get(%d)..\n", 
	     album_id);
  }
  else {
    print_folder(folder);
  }

  mps_library_folder_free(folder);
  
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

static void get_and_print_random(int count)
{
  int rv;
  mps_library_folder_t *folder[3];

  MPS_DBG_PRINT("Entering %s()\n", __func__);
  
  rv = mps_library_random(&folder[0], MPS_OBJECT_TYPE_TITLE, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_album_list_random(%d)..\n", count);
    goto OUT0;
  }
  rv = mps_library_random(&folder[1], MPS_OBJECT_TYPE_ALBUM, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_album_list_random(%d)..\n", count);
    goto OUT1;
  }
  rv = mps_library_random(&folder[2], MPS_OBJECT_TYPE_ARTIST, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_album_list_random(%d)..\n", count);
    goto OUT2;
  }

  print_folders(folder, 3);

 OUT2:
  mps_library_folder_free(folder[2]);
 OUT1:
  mps_library_folder_free(folder[1]);
 OUT0:
  mps_library_folder_free(folder[0]);
  
  MPS_DBG_PRINT("Leaving %s() rv = %d\n", __func__, rv);
}

static void get_and_print_search_results(char *search_string, int count)
{
  int rv;
  mps_library_folder_t *folder[4];

  MPS_DBG_PRINT("Entering %s(%s)\n", __func__, search_string);
  
  rv = mps_library_simple_search(&folder[0], search_string, 
                                 MPS_OBJECT_TYPE_TITLE, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_simple_search(%s, MPS_OBJECT_TYPE_TITLE)..\n", 
	     search_string);
    goto OUT0;
  }

  rv = mps_library_simple_search(&folder[1], search_string, 
                                 MPS_OBJECT_TYPE_ALBUM, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_simple_search(%s, MPS_OBJECT_TYPE_ALBUM)..\n", 
	     search_string);
    goto OUT1;
  }
  rv = mps_library_simple_search(&folder[2], search_string, 
                                 MPS_OBJECT_TYPE_ARTIST, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_simple_search(%s, MPS_OBJECT_TYPE_ARTIST)..\n", 
	     search_string);
    goto OUT2;
  }
  rv = mps_library_simple_search(&folder[3], search_string, 
                                 MPS_OBJECT_TYPE_FILE_PATH, count);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_simple_search(%s, MPS_OBJECT_TYPE_FILE_PATH)..\n", 
	     search_string);
    goto OUT3;
  }

  print_folders(folder, 4);

 OUT3:
  mps_library_folder_free(folder[3]);
 OUT2:
  mps_library_folder_free(folder[2]);
 OUT1:
  mps_library_folder_free(folder[1]);
 OUT0:
  mps_library_folder_free(folder[0]);
  
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

static void get_and_print_info(int title_id)
{
  int rv;
  mps_library_track_info_t info;
  MPS_DBG_PRINT("Entering %s(%d)\n", __func__, title_id);
  
  rv = mps_library_title_get_info(title_id, &info);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_title_get_info(%d)..\n", 
	     title_id);
  }
  else {
    char buffer[OUTPUT_BUFFER_SIZE];
    char *buffer_pointer=&buffer[0];
    int num_chars=0;
    int buffer_size=sizeof(buffer);
    mxml_node_t *xml, *ti, *title_id, *file_length, 
      *file_last_modification, *file_path, *file_type, *title, 
      *album, *album_id, *artist, *artist_id, *year, *genre, 
      *time_length, *play_count, *last_played, *ordinal_track_number, 
      *bitrate, *sampling_rate, *channel_mode, *vbr_mode;
    
    buffer[0]='\0';

    xml = mxmlNewXML("1.0");
    ti = mxmlNewElement(xml, "track_info");
    
    title_id = mxmlNewElement(ti, "title_id");
    mxmlElementSetAttrf(title_id, "value", "%ld", info.title_id);
    file_length = mxmlNewElement(ti, "file_length");
    mxmlElementSetAttrf(file_length, "value", "%ld", info.file_length);
    file_last_modification = mxmlNewElement(ti, "file_last_modification");
    mxmlElementSetAttrf(file_last_modification, "value", 
                        "%ld", info.file_last_modification);
    file_type = mxmlNewElement(ti, "file_type");
    mxmlElementSetAttrf
      (file_type, "value", "%s", 
       MPS_LIBRARY_ELEMENT_TYPE_TO_STRING(info.file_type));
    title = mxmlNewElement(ti, "title");
    mxmlElementSetAttrf(title, "value", "%s", info.title);
    album = mxmlNewElement(ti, "album");
    mxmlElementSetAttrf(album, "value", "%s", info.album);
    album_id = mxmlNewElement(ti, "album_id");
    mxmlElementSetAttrf(album_id, "value", "%ld", info.album_id);
    artist = mxmlNewElement(ti, "artist");
    mxmlElementSetAttrf(artist, "value", "%s", info.artist);
    artist_id = mxmlNewElement(ti, "artist_id");
    mxmlElementSetAttrf(artist_id, "value", "%ld", info.artist_id);
    year = mxmlNewElement(ti, "year");
    mxmlElementSetAttrf(year, "value", "%d", info.year);
    genre = mxmlNewElement(ti, "genre");
    mxmlElementSetAttrf(genre, "value", "%s", info.genre);
    time_length = mxmlNewElement(ti, "time_length");
    mxmlElementSetAttrf(time_length, "value", "%d", info.time_length);
    play_count = mxmlNewElement(ti, "play_count");
    mxmlElementSetAttrf(play_count, "value", "%d", info.play_count);
    last_played = mxmlNewElement(ti, "last_played");
    mxmlElementSetAttrf(last_played, "value", "%ld", info.last_played);
    ordinal_track_number = mxmlNewElement(ti, "ordinal_track_number");
    mxmlElementSetAttrf(ordinal_track_number, "value", "%d", 
                        info.ordinal_track_number);
    bitrate = mxmlNewElement(ti, "bitrate");
    mxmlElementSetAttrf(bitrate, "value", "%d", info.bitrate);
    sampling_rate = mxmlNewElement(ti, "sampling_rate");
    mxmlElementSetAttrf(sampling_rate, "value", "%d", info.sampling_rate);
    channel_mode = mxmlNewElement(ti, "channel_mode");
    mxmlElementSetAttrf
      (channel_mode, "value", "%s", 
       MPS_CHANNEL_MODE_T_TO_STRING(info.channel_mode));
    vbr_mode = mxmlNewElement(ti, "vbr_mode");
    mxmlElementSetAttrf
      (vbr_mode, "value", "%s", 
       MPS_VBR_MODE_T_TO_STRING(info.vbr_mode));      
    mxmlSetWrapMargin(0);
    mxmlSaveString(xml, buffer, sizeof(buffer), 
                   mps_command_whitespace_callback);	  

    printf("%s", buffer);
  }
 
  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}

static void get_and_print_status()
{
  int rv;
  char buffer[OUTPUT_BUFFER_SIZE];
  mps_library_status_t library_status;
  mxml_node_t *xml, *status, *param;

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  rv = mps_library_get_status(&library_status);
  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_get_status()..\n");
  }
  else {
    buffer[0]='\0';
    
    xml=mxmlNewXML("1.0");
    status=mxmlNewElement(xml, "status");

    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "change_counter");
    mxmlElementSetAttrf(param, "value", "%d", 
                        library_status.change_counter);

    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "librarydbd_state");
    mxmlElementSetAttrf
      (param, "value", "%s", 
       MPS_LIBRARYDB_STATE_T_TO_STRING(library_status.librarydbd_state));

    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "artist_count");
    mxmlElementSetAttrf(param, "value", "%ld", 
                        mps_library_get_artist_count());

    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "album_count");
    mxmlElementSetAttrf(param, "value", "%ld", 
                        mps_library_get_album_count());

    param=mxmlNewElement(status, "param");
    mxmlElementSetAttrf(param, "name", "title_count");
    mxmlElementSetAttrf(param, "value", "%ld", 
                        mps_library_get_title_count());

    MPS_DBG_PRINT("Preparing xml output\n");
    mxmlSetWrapMargin(0);
    mxmlSaveString(xml, buffer, sizeof(buffer), 
                   mps_command_whitespace_callback);	  
    
    mps_command_print_header();
    printf("%s", buffer);    
  }

  MPS_DBG_PRINT("Leaving %s()\n", __func__);
}


static void get_and_print_path(long path_id)
{
  int rv;
  mps_library_folder_t *folder;

  MPS_DBG_PRINT("Entering %s(%ld)\n", __func__, path_id);

  rv=mps_library_hierarchy_get(&folder, path_id);

  if (rv != MPS_SUCCESS) {
    MPS_LOG("Error returned from mps_library_hierarchy_get..\n");
  }
  else {
    print_folder(folder);
  }

  mps_library_folder_free(folder);

  MPS_DBG_PRINT("Leaving %s() rv = %d\n", __func__, rv);
}

static void print_folder(mps_library_folder_t *folder)
{
  mps_library_folder_t *folders[1];
  
  folders[0] = folder;
  
  print_folders(folders, 1);
}

static void print_folders(mps_library_folder_t **folders, int count)
{
  int i, j, rv, total_elements;
  mxml_node_t *xml, *xml_folder, *elements, *element;
  char buffer[1024*128];
  int buffer_size=sizeof(buffer);
  int save_string_rv;
  buffer[0]='\0';
   
  MPS_DBG_PRINT("Entering %s(0x%08x, %d)\n", __func__, (unsigned)folders, count);
    
  xml=mxmlNewXML("1.0");
  for (i=0, total_elements=0; i<count; i++) {
    total_elements += folders[i]->number_of_elements;
  }
  
  xml_folder=mxmlNewElement(xml, "folder");
  elements=mxmlNewElement(xml_folder, "elements");
  mxmlElementSetAttrf(elements, "count", "%d", total_elements);
  mxmlElementSetAttrf(elements, "parent", "%ld", folders[0]->parent);

  mxmlElementSetAttrf
    (elements, "name", "%s",
     folders[0]->name); 

  mxmlElementSetAttrf(elements, "id", "%ld", 
                      folders[0]->id);
  for (i=0; i<count; i++) {
    for (j=0; j<folders[i]->number_of_elements; j++) {
      element=mxmlNewElement(elements, "element");
      mxmlElementSetAttrf
        (element, "type", "%s",
         (MPS_IS_TITLE_ID(folders[i]->elements[j].id)?"FILE":"FOLDER"));
      if (MPS_IS_FILE_PATH_ID(folders[i]->elements[j].id)) {
        mxmlElementSetAttrf(element, "name", "/%s",  folders[i]->elements[j].name);
      }
      else {
        mxmlElementSetAttrf
          (element, "name", "%s",
           folders[i]->elements[j].name);         
      }
      mxmlElementSetAttrf(element, "id", "%ld", folders[i]->elements[j].id);
    }
  }
  mxmlSetWrapMargin(0);
  save_string_rv=mxmlSaveString(xml, buffer, sizeof(buffer), 
                                mps_command_whitespace_callback);
  if (save_string_rv > (buffer_size -1)) {
    MPS_LOG("error: message exceeds buffer_size (%d)\n", buffer_size);
    exit(1);
  }
  
  mps_command_print_header();
  printf("%s", buffer);
  
  MPS_DBG_PRINT("Leaving %s() rv = %d\n", __func__, rv);
}

