/*
 * Name: mps-command-shared.h
 *
 * Purpose: This file contains shared prototypes and definitions
 *          related to the "mps-commands" utilities.
 *
 */

#ifndef MPS_COMMAND_SHARED_H
#define MPS_COMMAND_SHARED_H

#include "libmpsserver.h"
#include <mxml.h>

#define OUTPUT_BUFFER_SIZE 65536

typedef struct mps_command_options {
  int            debug_enabled;
  int            debug_sink;
  bool           xml_output;
} mps_command_options_t;

extern mps_command_options_t *mps_command_options_p;
extern char **mps_command_tokens;
extern int mps_command_num_tokens;

extern int mps_command_init_and_getopts(int, char* []);
extern void mps_command_print_header();
extern const char* mps_command_whitespace_callback(mxml_node_t *, int);
extern int mps_utf8_isvalid(const unsigned char *input);
#endif /* define MPS_COMMAND_SHARED_H */
