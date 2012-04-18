/*
 * Name: mps-config.h
 *
 * Purpose: This file contains shared prototypes and definitions
 *          related to mps-config.
 *
 */
#ifndef MPS_CONFIG_H
#define MPS_CONFIG_H

#include <mxml.h>
#include <syslog.h>
#include "libmpsserver-private.h"

#define DEFAULT_MPSERVICE_CONF "/disk1/mpservice_configuration.xml"
#define DEFAULT_MPSERVICE_CONST "/mpservice/misc/mpservice_constants.xml"
 
typedef struct mps_config_options {
  int            debug_enabled;
  int            debug_sink;
  char           filename[512];
  bool           plain_text;  // As opposed to XML.
} mps_config_options_t;

#endif /* MPS_CONFIG_H */
