/*
 * mps-config.c - Implements the mps-config command.
 */

#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include "mps-config.h"

typedef enum {
  MPS_CONFIG = 0,
  MPS_CONSTANTS
} category_t;

int opt;
char **mps_config_tokens;
int mps_config_num_tokens;
category_t category = MPS_CONFIG;

// Declare the options struct with default values
// and set up a global pointer
mps_config_options_t mps_config_options =
  {
    0,            // debug_enabled
    DEBUG_TO_LOG,
    "",           // filename
    false,        // plain_text
  }; 
mps_config_options_t *mps_config_options_p = &mps_config_options;

static void usage(char *);
static void flatten_xml_file(char *filename);
static int set(char *, char *);
static int get(char *name[], int count);
static const char* mps_config_whitespace_callback(mxml_node_t *node, int where);

int main(int argc, char *argv[])
{
  int rv=MPS_SUCCESS;
  char config_filename[MPS_MAX_FILENAME_LENGTH];
  char command_basename[MPS_MAX_FILENAME_LENGTH];
  
  strcpy(command_basename, basename(argv[0]));

  if (!strcmp(command_basename, "mps-constants")) {
    category = MPS_CONSTANTS;
  }
	 
  while ((opt=getopt(argc, argv, "d:f:t")) != -1) {
    switch(opt) {
    case 'd':
      mps_config_options_p->debug_enabled = 1;
      mps_config_options_p->debug_sink = 
	((optarg[0] == 'c')?DEBUG_TO_CONSOLE:DEBUG_TO_LOG);	
      break;
    case 'f':
      strcpy(mps_config_options_p->filename, optarg);
      break;
    case 't':
      mps_config_options_p->plain_text = true;
      break;
    default:
      usage(command_basename);
      break;     
    }
  }

#ifdef DEBUG_ENABLED
  mps_debug_enabled = mps_config_options_p->debug_enabled;
  mps_debug_sink =mps_config_options_p->debug_sink;
#endif

  MPS_DBG_PRINT("options structure:\n");
  MPS_DBG_PRINT("mps_config_options = {\n");
  MPS_DBG_PRINT("  debug_enabled = %d\n", 
		 mps_config_options_p->debug_enabled);
  MPS_DBG_PRINT("  debug_sink = %d\n", mps_config_options_p->debug_sink);
  MPS_DBG_PRINT("  filename = %s\n", mps_config_options_p->filename);
  MPS_DBG_PRINT("  plain_text = %s\n", 
		 (mps_config_options_p->plain_text==true?"true":"false"));
  MPS_DBG_PRINT("}\n");

  mps_config_tokens=&argv[optind];
  mps_config_num_tokens=argc-optind;

  if (mps_config_num_tokens < 1) {
    usage(command_basename);
  }

  if (!strcmp(mps_config_tokens[0], "flatten")) {
    if (strcmp(mps_config_options_p->filename, "")) {
      strcpy(config_filename, mps_config_options_p->filename);
    }
    else {
      if (category == MPS_CONSTANTS) {
	mps_constants_get_filename(config_filename);
      }
      else {
	mps_configuration_get_filename(config_filename);
      }
    }
    flatten_xml_file(config_filename);
  }
  else if (!strcmp(mps_config_tokens[0], "set")) {
    if (mps_config_num_tokens == 3) {
      exit(set(mps_config_tokens[1], mps_config_tokens[2]));
    }
    else {
      usage(command_basename);
    }
  }
  else if (!strcmp(mps_config_tokens[0], "get")) {
    if (mps_config_num_tokens >= 2) {
      exit(get(&mps_config_tokens[1], mps_config_num_tokens - 1));
    }
    else {
      usage(command_basename);
    }
  }
  else {
    usage(command_basename);
  }
}

static void usage(char *command_name)
{
   printf("Usage:\n");
   printf("  %s flatten [ -d <c|l> ] [ -f filename ]\n", command_name);
   printf("  %s get <name 1> ... <name n> [-d <c|l> ] [ -f filename ]\n", 
	  command_name);
   if (category != MPS_CONSTANTS) {
     printf("  %s set <name> <value> [-d <c|l> ] [ -f filename ]\n", 
	    command_name);
   }
   printf("\n");
   exit(1);
}

static void flatten_xml_file(char *filename)
{
  FILE *fp;
  mxml_node_t *tree, *params, *node;
  const char *name, *value, *type;
  int i=0;

  fp = fopen(filename, "r");
  if (fp != NULL) {
    tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    if (tree != NULL) {
      params = mxmlFindElement(tree, tree, "parameters", 
			       NULL, NULL, MXML_DESCEND);

      if (params == NULL) {
	printf("Couldn't find <parameters> in %s\n", 
	       filename);
	MPS_LOG("Couldn't find <parameters> in %s\n", 
		 filename);
      }
      else {
	node = params;
	node = mxmlWalkNext(node, params, MXML_DESCEND);
	do {
	  if (node->type == MXML_ELEMENT) {
	    if (!strcmp(node->value.element.name, "param")) {
	      name = mxmlElementGetAttr(node, "name");
	      value = mxmlElementGetAttr(node, "value");
	      type = mxmlElementGetAttr(node, "type");
	      if (name == NULL || value == NULL || type == NULL) {
		printf("invalid line name=\"%s\" value=\"%s\" type=\"%s\"\n",
		       (name==NULL?"":name),
		       (value==NULL?"":value),
		       (type==NULL?"":type));
	      }
	      else {
		printf("%s='%s'\n",
		       name, value);
	      }
	    }
	  }
	} while ((node = mxmlWalkNext(node, params, MXML_NO_DESCEND))
		 != NULL); 
      }
    }
    else {
      printf("mxmlLoadFile couldn't open \"%s\"\n", filename);
      MPS_LOG("mxmlLoadFile couldn't open \"%s\"\n", filename);
    }
    mxmlDelete(tree);
    fclose(fp);
  }
  else {
    printf("Couldn't open \"%s\"\n", filename);
    MPS_LOG("Couldn't open \"%s\"\n", filename);
  }
}

static int set(char *name, char *value)
{
  mps_config_param_t param;
  int rv;

  strcpy(param.name, name);
  strcpy(param.value, value);

  if (category == MPS_CONSTANTS) {
    rv = mps_constants_set_param(&param);
  }
  else {
    rv = mps_configuration_set_param(&param);
  }

  if (rv != MPS_SUCCESS) {
    fprintf(stderr, "Error: Couldn't set \"%s\" to \"%s\"\n", 
	    param.name, param.value);
  }  
  return rv;
}

static int get(char *name[], int count)
{  
  mps_config_param_t params[count];
  int rv, i;
  char buffer[1024*32];
  mxml_node_t *xml, *parameters, *parameter;
  buffer[0]='\0';

  for (i=0; i < count; i++) {
    strcpy(params[i].name, name[i]);
  }
  xml=mxmlNewXML("1.0");
  parameters=mxmlNewElement(xml, "parameters");

  if (category == MPS_CONSTANTS) {
    rv = mps_constants_get_params(&params[0], count); 
  }
  else {
    rv = mps_configuration_get_params(&params[0], count);
  }

  if (rv == MPS_SUCCESS) {
    if (mps_config_options_p->plain_text) {
      for (i=0; i<count; i++) {
        printf("%s\n", params[i].value);
      }
    }
    else {
      for (i=0; i<count; i++) {
        parameter=mxmlNewElement(parameters, "param");
        mxmlElementSetAttrf(parameter, "name", "%s", params[i].name);
        mxmlElementSetAttrf(parameter, "value", "%s", params[i].value);
        mxmlElementSetAttrf(parameter, "type", "%s", params[i].type);
      }
      mxmlSetWrapMargin(0);
      if (mxmlSaveString(xml, buffer, sizeof(buffer), 
                         mps_config_whitespace_callback)
          > (sizeof(buffer) - 1)) {
        MPS_LOG("error: message exceeds buffer size (%d)\n", sizeof(buffer));
        rv=MPS_FAILURE;
        goto OUT;
      }
    }
  }
  else {
    MPS_LOG("Error: Couldn't find a value for \"%s\"...\"%s\"\n", 
            params[0].name, params[count-1].name);
    rv=MPS_FAILURE;
    goto OUT;
  }

 OUT:
  if (rv == MPS_SUCCESS) {
    if (!mps_config_options_p->plain_text) {
      printf("Content-type: text/xml; charset=utf-8\n");
      printf("Cache-Control: no-cache\n");
      printf("pragma: no-cache\n");
      printf("\n");
      printf("%s", buffer); 
    }
  }
  else {
    exit(1);
  }
}

static const char* mps_config_whitespace_callback(mxml_node_t *node, int where)
{
  const char *name;

  name = node->value.element.name;

  /* the top level xml tag */
  if (!strncmp(name, "?xml", 4)) {
    return ((where==MXML_WS_AFTER_OPEN)?"\n\n":NULL);
  }

  /* parameter messages */
  if (!strcmp(name, "parameters")) {
    return((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	   "\n":NULL);
  }
  if (!strcmp(name, "param")) {
    if (where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE) {
      if (!strcmp(node->parent->value.element.name, "parameters")) {
	return("  ");
      }
    }
    return ((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	    "\n":NULL);
  }

  return NULL;
}
