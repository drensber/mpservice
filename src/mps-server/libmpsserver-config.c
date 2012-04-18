#include "libmpsserver-private.h"

typedef enum {
  PARAM_SET,
  PARAM_GET
} action_type_t;

static int traverse_xml_tree(char *filename, action_type_t action,
			     mps_config_param_t *params, int number_of_params);

static const char* mps_command_whitespace_callback(mxml_node_t *node, 
						   int where);


int mps_configuration_get_filename(char *filename)
{
  int rv=MPS_SUCCESS;
  char *temp_char_ptr;

  MPS_DBG_PRINT("Entering %s(\"%s\")\n", __func__, filename);

  temp_char_ptr = getenv("MPSERVICE_CONFIGURATION_FILE");
  
  MPS_DBG_PRINT("getenv() returned \"%s\"\n", temp_char_ptr);

  if (temp_char_ptr != NULL) {
    strcpy(filename, temp_char_ptr); 
  }
  else {
    strcpy(filename, "/disk1/mpservice_configuration.xml");
  }

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_constants_get_filename(char *filename)
{
  int rv=MPS_SUCCESS;
  char *temp_char_ptr;

  MPS_DBG_PRINT("Entering %s(\"%s\")\n", __func__, filename);

  temp_char_ptr = getenv("MPSERVICE_CONSTANTS_FILE");
  
  MPS_DBG_PRINT("getenv() returned \"%s\"\n", temp_char_ptr);

  if (temp_char_ptr != NULL) {
    strcpy(filename, temp_char_ptr); 
  }
  else {
    strcpy(filename, "/mpservice/misc/mpservice_constants.xml");
  }

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_configuration_get_param(mps_config_param_t *param)
{
  int rv=MPS_SUCCESS;

  MPS_DBG_PRINT("Entering %s({\"%s\", 0x%08lx})\n", __func__, 
		 param->name, (unsigned long)param->value);
  
  rv = mps_configuration_get_params(param, 1);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_configuration_get_params(mps_config_param_t *params, int count)
{
  int rv=MPS_SUCCESS;
  char config_filename[MPS_MAX_FILENAME_LENGTH];

  MPS_DBG_PRINT("Entering %s()\n", __func__);

  mps_configuration_get_filename(config_filename);

  rv = traverse_xml_tree(config_filename, PARAM_GET, params, count);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_configuration_set_param(mps_config_param_t *param)
{
  int rv=MPS_SUCCESS;

  MPS_DBG_PRINT("Entering %s({\"%s\", \"%s\"})\n", __func__, 
		 param->name, param->value);
  
  rv = mps_configuration_set_params(param, 1);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_configuration_set_params(mps_config_param_t *params, int count)
{
  int rv=MPS_SUCCESS;
  char config_filename[MPS_MAX_FILENAME_LENGTH];

  MPS_DBG_PRINT("Entering %s({\"%s\", \"%s\"}... )\n", __func__,  
                params[0].name, params[0].value);

  mps_configuration_get_filename(config_filename);

  rv = traverse_xml_tree(config_filename, PARAM_SET, params, count);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_constants_get_param(mps_config_param_t *param)
{
  int rv=MPS_SUCCESS;

  MPS_DBG_PRINT("Entering %s({\"%s\"})\n", __func__, 
		 param->name);
  
  rv = mps_constants_get_params(param, 1);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;  
}

int mps_constants_get_params(mps_config_param_t *params, int count)
{
  int rv=MPS_SUCCESS;
  char constants_filename[MPS_MAX_FILENAME_LENGTH];

  MPS_DBG_PRINT("Entering %s(\"%s\"...)\n", __func__, params[0].name);

  mps_constants_get_filename(constants_filename);

  rv = traverse_xml_tree(constants_filename, PARAM_GET, params, count);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_constants_set_param(mps_config_param_t *param)
{
  int rv=MPS_SUCCESS;

  MPS_DBG_PRINT("Entering %s({\"%s\", \"%s\"})\n", __func__, 
		 param->name, param->value);

  rv = mps_constants_set_params(param, 1);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

int mps_constants_set_params(mps_config_param_t *params, int count)
{
  int rv=MPS_SUCCESS;
  char constants_filename[MPS_MAX_FILENAME_LENGTH];

  MPS_DBG_PRINT("Entering %s({\"%s\", \"%s\"}... )\n", __func__,  
                params[0].name, params[0].value);

  mps_constants_get_filename(constants_filename);

  rv = traverse_xml_tree(constants_filename, PARAM_SET, params, count);

  MPS_DBG_PRINT("Leaving %s() - return value %d\n", __func__, rv);
  return rv;
}

static int traverse_xml_tree(char *filename, action_type_t action,
			     mps_config_param_t *params, int number_of_params)
{
  int rv=MPS_FAILURE;

  FILE *fp, *temp_fp;
  mxml_node_t *xmlTree, *xmlParams, *xmlNode;
  const char *name, *value, *type;
  mps_config_param_t *param;
  char filename_full_path[MPS_MAX_FOLDERNAME_LENGTH],
    temp_file_name[MPS_MAX_FOLDERNAME_LENGTH];
  char *config_file_directory;
  int i=0, j;

  
  fp = fopen(filename, "r");
  if (fp != NULL) {
    xmlTree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    if (xmlTree != NULL) {
      xmlParams = mxmlFindElement(xmlTree, xmlTree, "parameters", 
			       NULL, NULL, MXML_DESCEND);

      if (xmlParams == NULL) {
	MPS_LOG("Couldn't find <parameters> %s\n", 
		 filename);
	rv=MPS_FAILURE;
      }
      else {
	xmlNode = xmlParams;
	xmlNode = mxmlWalkNext(xmlNode, xmlParams, MXML_DESCEND);
	do {
	  if (xmlNode->type == MXML_ELEMENT) {
	    if (!strcmp(xmlNode->value.element.name, "param")) {
	      name = mxmlElementGetAttr(xmlNode, "name");
	      value = mxmlElementGetAttr(xmlNode, "value");
	      type = mxmlElementGetAttr(xmlNode, "type");
	      if (name == NULL || value == NULL || type == NULL) {
		MPS_LOG("invalid line name=\"%s\" value=\"%s\" type=\"%s\"\n",
			 (name==NULL?"":name),
			 (value==NULL?"":value),
			 (type==NULL?"":type));
		rv=MPS_FAILURE;
		break;
	      }
	      else {
		MPS_DBG_PRINT("%s='%s'\n", name, value);
                
                for (j=0; j<number_of_params; j++) {
                  param = &params[j];
                  
                  MPS_DBG_PRINT("param->name=\"%s\"\n", param->name);
                  if (!strcmp(name, param->name)) {
                    if (action == PARAM_GET) {
                      strcpy(param->value, value);
                      strcpy(param->type, type);
                      rv=MPS_SUCCESS;
                      break;
                    }
                    else if (action == PARAM_SET) {
                      mxmlElementSetAttr(xmlNode, "value", param->value);
                      if (realpath(filename, filename_full_path) == NULL) {
                        MPS_LOG("Error: realpath(%s, 0x%08lx) returned NULL\n",
                                filename, (unsigned long)filename_full_path);
                      } 
                      else {
                        sprintf(temp_file_name, "%sXXXXXX", filename_full_path);
                        mktemp(temp_file_name);
                        temp_fp = fopen(temp_file_name, "w");
                        if (temp_fp == NULL) {
                          MPS_LOG("Error: fopen(%s, \"w\") returned NULL\n",
                                  temp_file_name);
                        }
                        else {
                          mxmlSetWrapMargin(0);
                          if (mxmlSaveFile(xmlTree, temp_fp, 
                                           mps_command_whitespace_callback)) {
                            MPS_LOG("Error: mxmlSaveFile() returned an error.\n");
                            fclose(temp_fp);
                          }
                          else {
                            fclose(temp_fp);
                            if (rename(temp_file_name, filename_full_path)) {
                              MPS_LOG("Error: rename(\"%s\", \"%s\") returned error.\n",
                                      temp_file_name, filename_full_path);
                            }
                            else {
                              rv=MPS_SUCCESS;
                            }
                          }
                        }
                      }
                      break;
                    }
                  }
                }
	      }
	    }
	  }
	} while ((xmlNode = mxmlWalkNext(xmlNode, xmlParams, MXML_NO_DESCEND))
		 != NULL); 
      }
    }
    else {
      MPS_LOG("mxmlLoadFile couldn't open \"%s\"\n", filename);
      rv=MPS_FAILURE;
    }
    mxmlDelete(xmlTree);
    fclose(fp);
  }
  else {
    MPS_LOG("Couldn't open \"%s\"\n", filename);
    rv=MPS_FAILURE;
  }

  return rv;
}

static const char* mps_command_whitespace_callback(mxml_node_t *node, int where)
{
  const char *name;
  name = node->value.element.name;

  /* the top level xml tag */
  if (!strncmp(name, "?xml", 4)) {
    return ((where==MXML_WS_AFTER_OPEN)?"\n\n":NULL);
  }

  if (!strcmp(name, "parameters")) {
    return((where==MXML_WS_AFTER_OPEN?"\n":
	    (where==MXML_WS_AFTER_CLOSE)?"\n\n":
	    NULL));
  }

  if (!strcmp(name, "param")) {
    if (where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE) {
      if (!strcmp(node->parent->value.element.name, "parameters")) {
	return("  ");
      }
    }
    return((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	   "\n":NULL);
  }

  if (!strncmp(name, "!--", 3)) {
    if (where==MXML_WS_BEFORE_OPEN || where==MXML_WS_BEFORE_CLOSE) {
      if (!strcmp(node->parent->value.element.name, "parameters")) {
	return("  ");
      }
    }
    return((where==MXML_WS_AFTER_OPEN || where==MXML_WS_AFTER_CLOSE)?
	   "\n":NULL);
  }

  return NULL;
}
