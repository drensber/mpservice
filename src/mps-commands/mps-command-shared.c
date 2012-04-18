/*
 * mps-command-shared.c -
 *   Contains objects that are shared by all of the "mps-commands" utilities.
 *
 */     
#include <stdio.h>
#include "mps-command-shared.h"

int opt;
char **mps_command_tokens;
int mps_command_num_tokens;

// Declare the options struct with default values
// and set up a global pointer
mps_command_options_t mps_command_options =
  {
    0,            // debug_enabled
    DEBUG_TO_LOG, // debug_sink
    false,        // xml_output
  }; 
mps_command_options_t *mps_command_options_p = &mps_command_options;

int mps_command_init_and_getopts(int argc, char *argv[])
{
  int rv=MPS_SUCCESS;
  int i;

  while ((opt=getopt(argc, argv, "hd:x")) != -1) {
    switch(opt) {
    case 'h':
      rv=MPS_FAILURE;
      break;
    case 'd':
      mps_command_options_p->debug_enabled=1;
      mps_command_options_p->debug_sink = 
	((optarg[0] == 'c')?DEBUG_TO_CONSOLE:DEBUG_TO_LOG);	
      break;
    case 'x':
      mps_command_options_p->xml_output=true;
      break;
    default:
      printf("unknown argument '%c'\n", opt);
      rv=MPS_FAILURE;
      break;
    }
  }

#ifdef DEBUG_ENABLED
  mps_debug_enabled=mps_command_options_p->debug_enabled;
  mps_debug_sink=mps_command_options_p->debug_sink;
#endif

  MPS_DBG_PRINT("options structure:\n");
  MPS_DBG_PRINT("mps_command_options = {\n");
  MPS_DBG_PRINT("  debug_enabled = %d\n", mps_command_options_p->debug_enabled);
  MPS_DBG_PRINT("  debug_sink = %d\n", mps_command_options_p->debug_sink);
  MPS_DBG_PRINT("}\n");

  mps_command_tokens=&argv[optind];
  mps_command_num_tokens=argc-optind;

  if (mps_initialize_client() < MPS_SUCCESS) {
    exit(1);
  }

  return rv;
}

void mps_command_print_header() {
  printf("Content-type: text/xml; charset=utf-8\n");
  printf("Cache-Control: no-cache\n");
  printf("pragma: no-cache\n");
  printf("\n");
}

/* this could be performance optimized by making separate
   versions for player, library, playlist.  Not worth it for now */
const char* mps_command_whitespace_callback(mxml_node_t *node, int where)
{
  const char *name;

  name = node->value.element.name;

  /* the top level xml tag */
  if (!strncmp(name, "?xml", 4)) {
    return ((where==MXML_WS_AFTER_OPEN)?"\n\n":NULL);
  }

  /* playlist messages */
  if (!strcmp(name, "playlist")) {
    return((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	   "\n":NULL);
  }
  if (!strcmp(name, "entries")) {
    if (where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE) {
      if (!strcmp(node->parent->value.element.name, "playlist")) {
	return("  ");
      }
    }
    return ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	    "\n":NULL);
  }
  if (!strcmp(name, "entry")) {
    return ((where==MXML_WS_BEFORE_OPEN)?"    ":
	    ((where==MXML_WS_AFTER_OPEN)?"\n":NULL));
  }

  /* command messages */
  if (!strcmp(name, "status") || !strcmp(name, "summary")) {
    return ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	    "\n":NULL);
  }

  if (!strcmp(name, "param") ) {
    return ((where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE)?
            "  ":
            "\n");
  }

  /* library messages */
  if (!strcmp(name, "folder")) {
    return ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	    "\n":NULL);
  }
  if (!strcmp(name, "elements")) {
    return ((where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE)?"  ":
	    ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?"\n":
	     NULL));
  }
  if (!strcmp(name, "element")) {
    return ((where==MXML_WS_BEFORE_OPEN)?"    ":
	    ((where==MXML_WS_AFTER_OPEN)?"\n":NULL));
  }

  if (!strcmp(name, "track_info")) {
    return 
      ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?"\n":NULL);
  }

  if (!strcmp(name, "title_id") || !strcmp(name, "file_length") ||
      !strcmp(name, "file_last_modification") || !strcmp(name, "file_path") ||
      !strcmp(name, "file_type") || !strcmp(name, "title") ||
      !strcmp(name, "album") || !strcmp(name, "album_id") ||
      !strcmp(name, "artist") || !strcmp(name, "artist_id") ||
      !strcmp(name, "year") || !strcmp(name, "genre") ||
      !strcmp(name, "time_length") || !strcmp(name, "play_count") ||
      !strcmp(name, "last_played") || !strcmp(name, "ordinal_track_number") ||
      !strcmp(name, "bitrate") || !strcmp(name, "sampling_rate") ||
      !strcmp(name, "channel_mode") || !strcmp(name, "vbr_mode")) {
    return ((where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE)?
	    "  ":"\n");
  }

  return NULL;
}

/* utf-8 validator */ 
int mps_utf8_isvalid(const unsigned char *input) 
{
  int nb, i;
  unsigned char *c;
  for (c = (unsigned char *)input;  *c;  c += (nb + 1)) {
    if (! (*c & 0x80)) {
      if ((*c > 0x00 && *c < 0x20) || *c == 0x7f) return 0; 
      nb = 0;
    }
    else {
      if ((*c & 0xc0) == 0x80) return 0;
      else if ((*c & 0xe0) == 0xc0) nb = 1;
      else if ((*c & 0xf0) == 0xe0) nb = 2;
      else if ((*c & 0xf8) == 0xf0) nb = 3;
      else if ((*c & 0xfc) == 0xf8) nb = 4;
      else if ((*c & 0xfe) == 0xfc) nb = 5;
      else return 0;
      for (i = 1; i < (nb + 1); i++) {
	if ((*(c + i) & 0xc0) != 0x80) {
	  return 0;
	}
      }
    }
  }
  return 1;
}
