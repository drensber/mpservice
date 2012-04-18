/*
	mpg123: main code of the program (not of the decoder...)

	copyright 1995-2009 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Michael Hipp
*/

#define ME "main"
#include "mpg123app.h"
#include "mpg123.h"
#include "local.h"

#include <sys/wait.h>
#include <sys/resource.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>

/* be paranoid about setpriority support */
#ifndef PRIO_PROCESS
#undef HAVE_SETPRIORITY
#endif

#include "common.h"
#include "getlopt.h"
#include "buffer.h"
#include "term.h"
#include "playlist.h"
#include "httpget.h"
#include "metaprint.h"
#include "httpget.h"

#include "debug.h"
#include "libmpsserver-private.h"

static void usage(int err);
static void want_usage(char* arg);
static void long_usage(int err);
static void want_long_usage(char* arg);
static void print_title(FILE* o);
static void give_version(char* arg);

struct parameter param = { 
  FALSE , /* aggressiv */
  FALSE , /* shuffle */
  FALSE , /* remote */
  FALSE , /* remote to stderr */
  DECODE_AUDIO , /* write samples to audio device */
  FALSE , /* silent operation */
  FALSE , /* xterm title on/off */
  0 ,     /* second level buffer size */
  0 ,     /* verbose level */
  DEFAULT_OUTPUT_MODULE,	/* output module */
  NULL,   /* output device */
  0,      /* destination (headphones, ...) */
  FALSE , /* term control */
  FALSE , /* checkrange */
  0 ,	  /* force_reopen, always (re)opens audio device for next song */
  /* test_cpu flag is valid for multi and 3dnow.. even if 3dnow is built alone; ensure it appears only once */
  FALSE , /* normal operation */
  FALSE,  /* try to run process in 'realtime mode' */
  NULL,  /* wav,cdr,au Filename */
	0, /* default is to play all titles in playlist */
	NULL, /* no playlist per default */
	0 /* condensed id3 per default */
	,0 /* list_cpu */
	,NULL /* cpu */ 
	,NULL
	,0 /* timeout */
	,1 /* loop */
	,0 /* delay */
	,0 /* index */
	/* Parameters for mpg123 handle, defaults are queried from library! */
	,0 /* down_sample */
	,0 /* rva */
	,0 /* halfspeed */
	,0 /* doublespeed */
	,0 /* start_frame */
	,-1 /* frame_number */
	,0 /* outscale */
	,0 /* flags */
	,0 /* force_rate */
	,1 /* ICY */
	,1024 /* resync_limit */
	,0 /* smooth */
	,0.0 /* pitch */
	,0 /* ignore_mime */
	,NULL /* proxyurl */
	,0 /* keep_open */
	,0 /* force_utf8 */
	,INDEX_SIZE
	,NULL /* force_encoding */
	,1. /* preload */
	,-1 /* preframes */
	,-1 /* gain */
};

mpg123_handle *mh = NULL;
off_t framenum;
off_t frames_left;
audio_output_t *ao = NULL;
txfermem *buffermem = NULL;
char *prgName = NULL;
/* ThOr: pointers are not TRUE or FALSE */
char *equalfile = NULL;
struct httpdata htd;
int fresh = TRUE;
int have_output = FALSE; /* If we are past the output init step. */

int buffer_fd[2];
int buffer_pid;
size_t bufferblock = 0;

static int intflag = FALSE;
static int skip_tracks = 0;
int OutputDescriptor;
static int filept = -1;
char *binpath; /* Path to myself. */

/* File-global storage of command line arguments.
   They may be needed for cleanup after charset conversion. */
static char **argv = NULL;
static int    argc = 0;

/* Cleanup marker to know that we intiialized libmpg123 already. */
static int cleanup_mpg123 = FALSE;

void set_intflag()
{
	debug("set_intflag TRUE");
	intflag = TRUE;
	skip_tracks = 0;
}

static void catch_interrupt(void)
{
        MPS_DBG_PRINT("Catching signal\n");
	intflag = TRUE;
}

/* oh, what a mess... */
void next_track(void)
{
	intflag = TRUE;
	++skip_tracks;
}

void prev_track(void)
{
	if(pl.pos > 2) pl.pos -= 2;
	else pl.pos = 0;

	next_track();
}

void safe_exit(int code)
{
	char *dummy, *dammy;
#ifdef HAVE_TERMIOS
	if(param.term_ctrl)
		term_restore();
#endif
	if(have_output) exit_output(ao, intflag);

	if(mh != NULL) mpg123_delete(mh);

	if(cleanup_mpg123) mpg123_exit();

	httpdata_free(&htd);

	/* It's ugly... but let's just fix this still-reachable memory chunk of static char*. */
	split_dir_file("", &dummy, &dammy);
	exit(code);
}

/* returns 1 if reset_audio needed instead */
static void set_output_module( char *arg )
{
	int i;
		
	/* Search for a colon and set the device if found */
	for(i=0; i< strlen( arg ); i++) {
		if (arg[i] == ':') {
			arg[i] = 0;
			param.output_device = &arg[i+1];
			debug1("Setting output device: %s", param.output_device);
			break;
		}	
	}
	/* Set the output module */
	param.output_module = arg;
	debug1("Setting output module: %s", param.output_module );
}

static void set_output_flag(int flag)
{
  if(param.output_flags <= 0) param.output_flags = flag;
  else param.output_flags |= flag;
}

static void set_output_h(char *a)
{
	set_output_flag(AUDIO_OUT_HEADPHONES);
}

static void set_output_s(char *a)
{
	set_output_flag(AUDIO_OUT_INTERNAL_SPEAKER);
}

static void set_output_l(char *a)
{
	set_output_flag(AUDIO_OUT_LINE_OUT);
}

static void set_output(char *arg)
{
	/* If single letter, it's the legacy output switch for AIX/HP/Sun.
	   If longer, it's module[:device] . If zero length, it's rubbish. */
	if(strlen(arg) <= 1) switch(arg[0])
	{
		case 'h': set_output_h(arg); break;
		case 's': set_output_s(arg); break;
		case 'l': set_output_l(arg); break;
		default:
			error1("\"%s\" is no valid output", arg);
			safe_exit(1);
	}
	else set_output_module(arg);
}

static void set_verbose (char *arg)
{
    param.verbose++;
}

static void set_debug (char *arg)
{
#ifdef DEBUG_ENABLED
    mps_debug_enabled=1;
    mps_debug_sink=DEBUG_TO_CONSOLE;
#else
    ;
#endif
}

static void set_quiet (char *arg)
{
	param.verbose=0;
	param.quiet=TRUE;
}

static void set_out_wav(char *arg)
{
	param.outmode = DECODE_WAV;
	param.filename = arg;
}

void set_out_cdr(char *arg)
{
	param.outmode = DECODE_CDR;
	param.filename = arg;
}

void set_out_au(char *arg)
{
  param.outmode = DECODE_AU;
  param.filename = arg;
}

static void set_out_file(char *arg)
{
	param.outmode=DECODE_FILE;
	OutputDescriptor=open(arg,O_CREAT|O_WRONLY|O_TRUNC,0666);
	if(OutputDescriptor==-1)
	{
		error2("Can't open %s for writing (%s).\n",arg,strerror(errno));
		safe_exit(1);
	}
}

static void set_out_stdout(char *arg)
{
	param.outmode=DECODE_FILE;
	param.remote_err=TRUE;
	OutputDescriptor=STDOUT_FILENO;
}

static void set_out_stdout1(char *arg)
{
	param.outmode=DECODE_AUDIOFILE;
	param.remote_err=TRUE;
	OutputDescriptor=STDOUT_FILENO;
}

#ifndef HAVE_SCHED_SETSCHEDULER
static void realtime_not_compiled(char *arg)
{
	MPS_DBG_PRINT("Option '-T / --realtime' not compiled into this binary.\n");
}
#endif

static int frameflag; /* ugly, but that's the way without hacking getlopt */
static void set_frameflag(char *arg)
{
	/* Only one mono flag at a time! */
	if(frameflag & MPG123_FORCE_MONO) param.flags &= ~MPG123_FORCE_MONO;
	param.flags |= frameflag;
}
static void unset_frameflag(char *arg)
{
	param.flags &= ~frameflag;
}
/* Please note: GLO_NUM expects point to LONG! */
/* ThOr:
 *  Yeah, and despite that numerous addresses to int variables were 
passed.
 *  That's not good on my Alpha machine with int=32bit and long=64bit!
 *  Introduced GLO_INT and GLO_LONG as different bits to make that clear.
 *  GLO_NUM no longer exists.
 */
#ifdef OPT_3DNOW
static int dnow = 0; /* helper for mapping the old 3dnow options */
#endif
topt opts[] = {
	{'k', "skip",        GLO_ARG | GLO_LONG, 0, &param.start_frame, 0},
	{'2', "2to1",        GLO_INT,  0, &param.down_sample, 1},
	{'4', "4to1",        GLO_INT,  0, &param.down_sample, 2},
	{'t', "test",        GLO_INT,  0, &param.outmode, DECODE_TEST},
	{'s', "stdout",      GLO_INT,  set_out_stdout, &param.outmode, DECODE_FILE},
	{'S', "STDOUT",      GLO_INT,  set_out_stdout1, &param.outmode,DECODE_AUDIOFILE},
	{'O', "outfile",     GLO_ARG | GLO_CHAR, set_out_file, NULL, 0},
	{'c', "check",       GLO_INT,  0, &param.checkrange, TRUE},
	{'v', "verbose",     0,        set_verbose, 0,           0},
	{'q', "quiet",       0,        set_quiet,   0,           0},
	{'y', "no-resync",      GLO_INT,  set_frameflag, &frameflag, MPG123_NO_RESYNC},
	/* compatibility, no-resync is to be used nowadays */
	{0, "resync",      GLO_INT,  set_frameflag, &frameflag, MPG123_NO_RESYNC},
	{'0', "single0",     GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_LEFT},
	{0,   "left",        GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_LEFT},
	{'1', "single1",     GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_RIGHT},
	{0,   "right",       GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_RIGHT},
	{'m', "singlemix",   GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_MIX},
	{0,   "mix",         GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_MIX},
	{0,   "mono",        GLO_INT,  set_frameflag, &frameflag, MPG123_MONO_MIX},
	{0,   "stereo",      GLO_INT,  set_frameflag, &frameflag, MPG123_FORCE_STEREO},
	{0,   "reopen",      GLO_INT,  0, &param.force_reopen, 1},
	{'g', "gain",        GLO_ARG | GLO_LONG, 0, &param.gain,    0},
	{'r', "rate",        GLO_ARG | GLO_LONG, 0, &param.force_rate,  0},
	{0,   "8bit",        GLO_INT,  set_frameflag, &frameflag, MPG123_FORCE_8BIT},
	{0,   "float",       GLO_INT,  set_frameflag, &frameflag, MPG123_FORCE_FLOAT},
	{0,   "headphones",  0,                  set_output_h, 0,0},
	{0,   "speaker",     0,                  set_output_s, 0,0},
	{0,   "lineout",     0,                  set_output_l, 0,0},
	{'o', "output",      GLO_ARG | GLO_CHAR, set_output, 0,  0},
	{0,   "list-modules",0,        list_modules, NULL,  0}, 
	{'a', "audiodevice", GLO_ARG | GLO_CHAR, 0, &param.output_device,  0},
	{'f', "scale",       GLO_ARG | GLO_LONG, 0, &param.outscale,   0},
	{'n', "frames",      GLO_ARG | GLO_LONG, 0, &param.frame_number,  0},
	#ifdef HAVE_TERMIOS
	{'C', "control",     GLO_INT,  0, &param.term_ctrl, TRUE},
	#endif
	{'R', "remote",      GLO_INT,  0, &param.remote, TRUE},
	{0,   "remote-err",  GLO_INT,  0, &param.remote_err, TRUE},
	{'d', "doublespeed", GLO_ARG | GLO_LONG, 0, &param.doublespeed, 0},
	{'h', "halfspeed",   GLO_ARG | GLO_LONG, 0, &param.halfspeed, 0},
	{'p', "proxy",       GLO_ARG | GLO_CHAR, 0, &param.proxyurl,   0},
	{'@', "list",        GLO_ARG | GLO_CHAR, 0, &param.listname,   0},
	/* 'z' comes from the the german word 'zufall' (eng: random) */
	{'z', "shuffle",     GLO_INT,  0, &param.shuffle, 1},
	{'Z', "random",      GLO_INT,  0, &param.shuffle, 2},
	{'E', "equalizer",	 GLO_ARG | GLO_CHAR, 0, &equalfile,1},
	#ifdef HAVE_SETPRIORITY
	{0,   "aggressive",	 GLO_INT,  0, &param.aggressive, 2},
	#endif
	#ifdef OPT_3DNOW
#define SET_3DNOW 1
#define SET_I586  2
	{0,   "force-3dnow", GLO_CHAR,  0, &dnow, SET_3DNOW},
	{0,   "no-3dnow",    GLO_CHAR,  0, &dnow, SET_I586},
	{0,   "test-3dnow",  GLO_INT,  0, &param.test_cpu, TRUE},
	#endif
	{0, "cpu", GLO_ARG | GLO_CHAR, 0, &param.cpu,  0},
	{0, "test-cpu",  GLO_INT,  0, &param.test_cpu, TRUE},
	{0, "list-cpu", GLO_INT,  0, &param.list_cpu , 1},
	#ifdef NETWORK
	{'u', "auth",        GLO_ARG | GLO_CHAR, 0, &httpauth,   0},
	#endif
	#if defined (HAVE_SCHED_SETSCHEDULER)
	/* check why this should be a long variable instead of int! */
	{'T', "realtime",    GLO_LONG,  0, &param.realtime, TRUE },
	#else
	{'T', "realtime",    0,  realtime_not_compiled, 0,           0 },    
	#endif
	{0, "title",         GLO_INT,  0, &param.xterm_title, TRUE },
	{'w', "wav",         GLO_ARG | GLO_CHAR, set_out_wav, 0, 0 },
	{0, "cdr",           GLO_ARG | GLO_CHAR, set_out_cdr, 0, 0 },
	{0, "au",            GLO_ARG | GLO_CHAR, set_out_au, 0, 0 },
	{0,   "gapless",	 GLO_INT,  set_frameflag, &frameflag, MPG123_GAPLESS},
	{0,   "no-gapless", GLO_INT, unset_frameflag, &frameflag, MPG123_GAPLESS},
	{'?', "help",            0,  want_usage, 0,           0 },
	{0 , "longhelp" ,        0,  want_long_usage, 0,      0 },
	{0 , "version" ,         0,  give_version, 0,         0 },
	{'l', "listentry",       GLO_ARG | GLO_LONG, 0, &param.listentry, 0 },
	{0, "rva-mix",         GLO_INT,  0, &param.rva, 1 },
	{0, "rva-radio",         GLO_INT,  0, &param.rva, 1 },
	{0, "rva-album",         GLO_INT,  0, &param.rva, 2 },
	{0, "rva-audiophile",         GLO_INT,  0, &param.rva, 2 },
	{0, "no-icy-meta",      GLO_INT,  0, &param.talk_icy, 0 },
	{0, "long-tag",         GLO_INT,  0, &param.long_id3, 1 },
	{0, "fifo", GLO_ARG | GLO_CHAR, 0, &param.fifo,  0},
	{0, "timeout", GLO_ARG | GLO_LONG, 0, &param.timeout, 0},
	{0, "loop", GLO_ARG | GLO_LONG, 0, &param.loop, 0},
	{'i', "index", GLO_INT, 0, &param.index, 1},
	{'D', "delay", GLO_ARG | GLO_INT, 0, &param.delay, 0},
	{0, "resync-limit", GLO_ARG | GLO_LONG, 0, &param.resync_limit, 0},
	{0, "pitch", GLO_ARG|GLO_DOUBLE, 0, &param.pitch, 0},
	{0, "ignore-mime", GLO_INT,  0, &param.ignore_mime, 1 },
	{0, "keep-open", GLO_INT, 0, &param.keep_open, 1},
	{0, "utf8", GLO_INT, 0, &param.force_utf8, 1},
	{0, "fuzzy", GLO_INT,  set_frameflag, &frameflag, MPG123_FUZZY},
	{0, "index-size", GLO_ARG|GLO_LONG, 0, &param.index_size, 0},
	{0, "no-seekbuffer", GLO_INT, unset_frameflag, &frameflag, MPG123_SEEKBUFFER},
	{'e', "encoding", GLO_ARG|GLO_CHAR, 0, &param.force_encoding, 0},
	{0, "preload", GLO_ARG|GLO_DOUBLE, 0, &param.preload, 0},
	{0, "preframes", GLO_ARG|GLO_LONG, 0, &param.preframes, 0},
	{'B', "debug enabled",     0,        set_debug, 0,           0},
	{0, 0, 0, 0, 0, 0}
};

/*
 *   Change the playback sample rate.
 *   Consider that changing it after starting playback is not covered by gapless code!
 */
static void reset_audio(long rate, int channels, int format)
{
		if(ao == NULL)
		{
			error("Audio handle should not be NULL here!");
			safe_exit(98);
		}
		ao->rate     = pitch_rate(rate); 
		ao->channels = channels; 
		ao->format   = format;
		if(reset_output(ao) < 0)
		{
			error1("failed to reset audio device: %s", strerror(errno));
			safe_exit(1);
		}
}

/* 1 on success, 0 on failure */
int open_track(char *fname)
{
	filept=-1;
	httpdata_reset(&htd);
	if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, 0, 0))
	error1("Cannot (re)set ICY interval: %s", mpg123_strerror(mh));
	if(!strcmp(fname, "-"))
	{
		filept = STDIN_FILENO;
	}
	else if (!strncmp(fname, "http://", 7)) /* http stream */
	{
		filept = http_open(fname, &htd);
		/* now check if we got sth. and if we got sth. good */
		if(    (filept >= 0) && (htd.content_type.p != NULL)
			  && !param.ignore_mime && !(debunk_mime(htd.content_type.p) & IS_FILE) )
		{
			error1("Unknown mpeg MIME type %s - is it perhaps a playlist (use -@)?", htd.content_type.p == NULL ? "<nil>" : htd.content_type.p);
			error("If you know the stream is mpeg1/2 audio, then please report this as "PACKAGE_NAME" bug");
			return 0;
		}
		if(filept < 0)
		{
			error1("Access to http resource %s failed.", fname);
			return 0;
		}
		if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, htd.icy_interval, 0))
		error1("Cannot set ICY interval: %s", mpg123_strerror(mh));
		if(param.verbose > 1) {
		  MPS_DBG_PRINT( "Info: ICY interval %li\n", (long)htd.icy_interval);
		}
	}
	debug("OK... going to finally open.");
	/* Now hook up the decoder on the opened stream or the file. */
	if(filept > -1)
	{
		if(mpg123_open_fd(mh, filept) != MPG123_OK)
		{
			error2("Cannot open fd %i: %s", filept, mpg123_strerror(mh));
			return 0;
		}
	}
	else if(mpg123_open(mh, fname) != MPG123_OK)
	{
		error2("Cannot open %s: %s", fname, mpg123_strerror(mh));
		return 0;
	}
	debug("Track successfully opened.");
	fresh = TRUE;
	return 1;
}

/* for symmetry */
void close_track(void)
{
	mpg123_close(mh);
	if(filept > -1) close(filept);
	filept = -1;
}

/* return 1 on success, 0 on failure */
int play_frame(void)
{
	unsigned char *audio;
	int mc;
	size_t bytes;
	debug("play_frame");
	/* The first call will not decode anything but return MPG123_NEW_FORMAT! */
	mc = mpg123_decode_frame(mh, &framenum, &audio, &bytes);
	/* Play what is there to play (starting with second decode_frame call!) */
	if(bytes)
	{
		if(param.frame_number > -1) --frames_left;
		if(fresh && framenum >= param.start_frame)
		{
			fresh = FALSE;
			if(!param.quiet)
			{
				if(param.verbose) print_header(mh);
#if 0
				else print_header_compact(mh);
#endif
			}
		}
		/* Normal flushing of data, includes buffer decoding. */
		if(flush_output(ao, audio, bytes) < (int)bytes && !intflag)
		{
			error("Deep trouble! Cannot flush to my output anymore!");
			safe_exit(133);
		}
		if(param.checkrange)
		{
			long clip = mpg123_clip(mh);
			if(clip > 0) MPS_DBG_PRINT("%ld samples clipped\n", clip);
		}
	}
	/* Special actions and errors. */
	if(mc != MPG123_OK)
	{
		if(mc == MPG123_ERR || mc == MPG123_DONE)
		{
			if(mc == MPG123_ERR) error1("...in decoding next frame: %s", mpg123_strerror(mh));
			return 0;
		}
		if(mc == MPG123_NO_SPACE)
		{
			error("I have not enough output space? I didn't plan for this.");
			return 0;
		}
		if(mc == MPG123_NEW_FORMAT)
		{
			long rate;
			int channels, format;
			mpg123_getformat(mh, &rate, &channels, &format);
			if(param.verbose > 2) {
			  MPS_DBG_PRINT( "Note: New output format %liHz %ich, format %i\n", 
					 rate, channels, format);
			}

			reset_audio(rate, channels, format);
		}
	}
	return 1;
}

int main(int sys_argc, char ** sys_argv)
{
	int result;
	long parr;
	int libpar = 0;
	mpg123_pars *mp;
	struct timeval start_time;
	char fname[1024];
	long tmp_entry_id = MPS_PLAYLIST_EMPTY;
	int init;
	mps_skip_t skipFlag = SKIP_NEXT;
	
	/*initialize status and streamstatus files*/
	MPS_DBG_PRINT("Initializing shared memory\n");
	if (initialize_shared_memory_server() 
	    < MPS_SUCCESS) {
	  exit(1);
	}
	
	argv = sys_argv;
	argc = sys_argc;


	/* Need to initialize mpg123 lib here for default parameter values. */

	result = mpg123_init();
	if(result != MPG123_OK)
	{
		error1("Cannot initialize mpg123 library: %s", mpg123_plain_strerror(result));
		safe_exit(77);
	}
	cleanup_mpg123 = TRUE;

	mp = mpg123_new_pars(&result); /* This may get leaked on premature exit(), which is mainly a cosmetic issue... */
	if(mp == NULL)
	{
		error1("Crap! Cannot get mpg123 parameters: %s", mpg123_plain_strerror(result));
		safe_exit(78);
	}

	/* get default values */
	mpg123_getpar(mp, MPG123_DOWN_SAMPLE, &parr, NULL);
	param.down_sample = (int) parr;
	mpg123_getpar(mp, MPG123_RVA, &param.rva, NULL);
	mpg123_getpar(mp, MPG123_DOWNSPEED, &param.halfspeed, NULL);
	mpg123_getpar(mp, MPG123_UPSPEED, &param.doublespeed, NULL);
	mpg123_getpar(mp, MPG123_OUTSCALE, &param.outscale, NULL);
	mpg123_getpar(mp, MPG123_FLAGS, &parr, NULL);
	mpg123_getpar(mp, MPG123_INDEX_SIZE, &param.index_size, NULL);
	param.flags = (int) parr;
	param.flags |= MPG123_SEEKBUFFER; /* Default on, for HTTP streams. */
	mpg123_getpar(mp, MPG123_RESYNC_LIMIT, &param.resync_limit, NULL);
	mpg123_getpar(mp, MPG123_PREFRAMES, &param.preframes, NULL);

	while ((result = getlopt(argc, argv, opts)))
	switch (result) {
		case GLO_UNKNOWN:
			fprintf (stderr, "%s: Unknown option \"%s\".\n", 
				prgName, loptarg);
			usage(1);
		case GLO_NOARG:
			fprintf (stderr, "%s: Missing argument for option \"%s\".\n",
				prgName, loptarg);
			usage(1);
	}
	/* Do this _after_ parameter parsing. */
	check_locale(); /* Check/set locale; store if it uses UTF-8. */

	if(param.list_cpu)
	{
		const char **all_dec = mpg123_decoders();
		printf("Builtin decoders:");
		while(*all_dec != NULL){ printf(" %s", *all_dec); ++all_dec; }
		printf("\n");
		mpg123_delete_pars(mp);
		return 0;
	}
	if(param.test_cpu)
	{
		const char **all_dec = mpg123_supported_decoders();
		printf("Supported decoders:");
		while(*all_dec != NULL){ printf(" %s", *all_dec); ++all_dec; }
		printf("\n");
		mpg123_delete_pars(mp);
		return 0;
	}
	if(param.gain != -1)
	{
	    warning("The parameter -g is deprecated and may be removed in the future.");
	}

	/* Init audio as early as possible.
	   If there is the buffer process to be spawned, it shouldn't carry the mpg123_handle with it. */
	bufferblock = mpg123_safe_buffer(); /* Can call that before mpg123_init(), it's stateless. */
	if(init_output(&ao) < 0)
	{
		error("Failed to initialize output, goodbye.");
		mpg123_delete_pars(mp);
		return 99; /* It's safe here... nothing nasty happened yet. */
	}
	have_output = TRUE;

	/* ========================================================================================================= */
	/* Enterning the leaking zone... we start messing with stuff here that should be taken care of when leaving. */
	/* Don't just exit() or return out...                                                                        */
	/* ========================================================================================================= */

	httpdata_init(&htd);

	if (param.remote)
	{
		param.verbose = 0;
		param.quiet = 1;
		param.flags |= MPG123_QUIET;
	}

	/* Set the frame parameters from command line options */
	if(param.quiet) param.flags |= MPG123_QUIET;

#ifdef OPT_3DNOW
	if(dnow != 0) param.cpu = (dnow == SET_3DNOW) ? "3dnow" : "i586";
#endif
	if(param.cpu != NULL && (!strcmp(param.cpu, "auto") || !strcmp(param.cpu, ""))) param.cpu = NULL;
	if(!(  MPG123_OK == (result = mpg123_par(mp, MPG123_VERBOSE, param.verbose, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_FLAGS, param.flags, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_DOWN_SAMPLE, param.down_sample, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_RVA, param.rva, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_FORCE_RATE, param.force_rate, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_DOWNSPEED, param.halfspeed, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_UPSPEED, param.doublespeed, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_ICY_INTERVAL, 0, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_RESYNC_LIMIT, param.resync_limit, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_TIMEOUT, param.timeout, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_OUTSCALE, param.outscale, 0))
	    && ++libpar
	    && MPG123_OK == (result = mpg123_par(mp, MPG123_PREFRAMES, param.preframes, 0))
			))
	{
		error2("Cannot set library parameter %i: %s", libpar, mpg123_plain_strerror(result));
		safe_exit(45);
	}
	if (!(param.listentry < 0) && !param.quiet) print_title(stderr); /* do not pollute stdout! */

	{
		long default_index;
		mpg123_getpar(mp, MPG123_INDEX_SIZE, &default_index, NULL);
		if( param.index_size != default_index && (result = mpg123_par(mp, MPG123_INDEX_SIZE, param.index_size, 0.)) != MPG123_OK )
		error1("Setting of frame index size failed: %s", mpg123_plain_strerror(result));
	}

	if(param.force_rate && param.down_sample)
	{
		error("Down sampling and fixed rate options not allowed together!");
		safe_exit(1);
	}

	/* Now actually get an mpg123_handle. */
	mh = mpg123_parnew(mp, param.cpu, &result);
	if(mh == NULL)
	{
		error1("Crap! Cannot get a mpg123 handle: %s", mpg123_plain_strerror(result));
		safe_exit(77);
	}
	mpg123_delete_pars(mp); /* Don't need the parameters anymore ,they're in the handle now. */

	/* Now either check caps myself or query buffer for that. */
	audio_capabilities(ao, mh);

	load_equalizer(mh);

#ifdef HAVE_SETPRIORITY
	if(param.aggressive) { /* tst */
		int mypid = getpid();
		setpriority(PRIO_PROCESS,mypid,-20);
	}
#endif

#if defined (HAVE_SCHED_SETSCHEDULER) && !defined (__CYGWIN__)
/* Cygwin --realtime seems to fail when accessing network, using win32 set priority instead */
	if (param.realtime) {  /* Get real-time priority */
	  struct sched_param sp;
	  MPS_DBG_PRINT("Getting real-time priority\n");
	  memset(&sp, 0, sizeof(struct sched_param));
	  sp.sched_priority = sched_get_priority_min(SCHED_FIFO);
	  if (sched_setscheduler(0, SCHED_RR, &sp) == -1)
	    MPS_DBG_PRINT("Can't get real-time priority\n");
	}
#endif


        catchsignal(SIGUSR1, catch_interrupt);

#ifdef HAVE_TERMIOS
		debug1("param.term_ctrl: %i", param.term_ctrl);
		if(param.term_ctrl)
			term_init();
#endif
	while (1)
	{
		long current_title_id;
		int playlist_count;

		if(intflag) {
		  MPS_DBG_PRINT("Signal received in outer while loop.  opcode=%s\n",
				MPS_OPCODE_T_TO_STRING(mps_shm_region->control.opcode));
		  if (mps_shm_region->control.opcode == OPCODE_PLAY) {
		    mps_shm_region->player_status.state=STATE_PLAYING;
		    skipFlag=mps_shm_region->control.skip_method;
		  }
		  else if (mps_shm_region->control.opcode == OPCODE_STOP) {
		    mps_shm_region->player_status.state=STATE_STOPPED;
		    mps_shm_region->player_status.current_playlist_entry_id
		      = MPS_PLAYLIST_BEGINNING; // Reset to the beginning of playlist
		  }
		  else if (mps_shm_region->control.opcode == OPCODE_PAUSE) {
		    mps_shm_region->player_status.state=STATE_PAUSED;
		  }
		  
		  intflag=FALSE;
		}

		if (mps_shm_region->player_status.state==STATE_PLAYING) {
		  
		  if(skipFlag==SKIP_NEXT) {
		    
		    if (mps_shm_region->player_status.current_playlist_entry_id
			== MPS_PLAYLIST_END) {
		      tmp_entry_id=
			mps_playlist_get_next_id(mps_shm_region->player_status.previous_playlist_entry_id);
		    }
		    else {  
		      tmp_entry_id=
			mps_playlist_get_next_id(mps_shm_region->player_status.current_playlist_entry_id);
		      // We're about to change, so save the previous one (unless we're at the end).
		      mps_shm_region->player_status.previous_playlist_entry_id=
			mps_shm_region->player_status.current_playlist_entry_id;
		    }
		    mps_shm_region->player_status.current_playlist_entry_id=
		      tmp_entry_id;
		  }
		  else if (skipFlag==SKIP_PREVIOUS) {
		    tmp_entry_id =
		      mps_playlist_get_previous_id(mps_shm_region->player_status.current_playlist_entry_id);
		    mps_shm_region->player_status.current_playlist_entry_id =
		      tmp_entry_id;
		  } 
		  else if (skipFlag==SKIP_ABSOLUTE) {
		    mps_shm_region->player_status.current_playlist_entry_id=
		      mps_shm_region->control.playlist_entry_id;
		  }
		}

		// the default behavior (play the next song)
		skipFlag = SKIP_NEXT; 
		if (mps_shm_region->player_status.element_type != MPS_LIBRARY_MP3_STREAM &&
		    skipFlag == SKIP_NEXT &&
		    (mps_shm_region->player_status.previous_playlist_entry_id
		     == mps_playlist_get_last_id()) &&
		    mps_shm_region->player_status.current_playlist_entry_id == MPS_PLAYLIST_END) {
		  MPS_DBG_PRINT("Reached the end of the playlist\n");
		  usleep(250000);
		  continue;
		}
		
		if (mps_shm_region->player_status.element_type 
		    != MPS_LIBRARY_MP3_STREAM &&
		    (playlist_count = mps_playlist_get_count()) == 0) {
		  MPS_DBG_PRINT("Playlist is empty\n");
		  usleep(250000);
		  continue;
		}
		else {
		  MPS_DBG_PRINT("playlist_count == %d\n", playlist_count);
		}
		
		/* don't do anything if in STOP state */
		if (mps_shm_region->player_status.state != STATE_PLAYING) {
		  usleep(250000);
		  continue;
		}

		if(mps_shm_region->player_status.element_type 
		   == MPS_LIBRARY_MP3_STREAM) {
		  strcpy(fname, mps_shm_region->control.stream_file_name);
		  MPS_DBG_PRINT("Playing http stream file... fname=\"%s\"\n", fname);
		}
		else {
		  MPS_DBG_PRINT
		    ("Looking up title_id from current_playlist_entry_id=%ld\n",
		     mps_shm_region->player_status.current_playlist_entry_id);
		  current_title_id = mps_playlist_get_title_id_from_entry_id
		    (mps_shm_region->player_status.current_playlist_entry_id);
		  MPS_DBG_PRINT
		    ("get_title_id_from_entry_id converted to title_id=%ld\n",
		     current_title_id);
		  mps_library_get_file_path_from_id(current_title_id, fname);
		  MPS_DBG_PRINT("Playing mp3 file... fname=\"%s\"\n", fname);
		}

		frames_left = param.frame_number;
		MPS_DBG_PRINT("Going to play %s\n", fname);

		if(!open_track(fname))
		{
			continue;
		}

		if(!param.quiet) {
		  MPS_DBG_PRINT( "\n");
		}
		if(param.index)
		{
			if(param.verbose) MPS_DBG_PRINT( "indexing...\r");
			mpg123_scan(mh);
		}
		/*
			Only trigger a seek if we do not want to start with the first frame.
			Rationale: Because of libmpg123 sample accuracy, this could cause an unnecessary backwards seek, that even may fail on non-seekable streams.
			For start frame of 0, we are already at the correct position!
		*/
		framenum = 0;
		if(param.start_frame > 0)
		framenum = mpg123_seek_frame(mh, param.start_frame, SEEK_SET);

		if(framenum < 0)
		{
			error1("Initial seek failed: %s", mpg123_strerror(mh));
			if(mpg123_errcode(mh) == MPG123_BAD_OUTFORMAT)
			{
			        MPS_DBG_PRINT( "%s", "So, you have trouble getting an output format... this is the matrix of currently possible formats:\n");
				print_capabilities(ao, mh);
				MPS_DBG_PRINT( "%s", "Somehow the input data and your choices don't allow one of these.\n");
			}
			mpg123_close(mh);
			continue;
		}

		if (!param.quiet)
		{

#ifdef HAVE_TERMIOS
		/* Reminder about terminal usage. */
		if(param.term_ctrl) term_hint();
#endif


			MPS_DBG_PRINT( "Playing MPEG stream %lu of %lu ...\n", 
				       (unsigned long)pl.pos, (unsigned long)pl.fill);
			if(htd.icy_name.fill) MPS_DBG_PRINT( "ICY-NAME: %s\n", htd.icy_name.p);
			if(htd.icy_url.fill)  MPS_DBG_PRINT( "ICY-URL: %s\n",  htd.icy_url.p);

#if !defined(GENERIC)
{
	const char *term_type;
	term_type = getenv("TERM");
	if (term_type && param.xterm_title &&
	    (!strncmp(term_type,"xterm",5) || !strncmp(term_type,"rxvt",4)))
	{
		MPS_DBG_PRINT( "\033]0;%s\007", fname);
	}
}
#endif

		}
/* Rethink that SIGINT logic... */
#if !defined(GENERIC)
#ifdef HAVE_TERMIOS
		if(!param.term_ctrl)
#endif
			gettimeofday (&start_time, NULL);
#endif

		while(1)
		{
		  if (intflag) {
		    MPS_DBG_PRINT("Received signal in inner loop.  opcode=%s\n", 
				  MPS_OPCODE_T_TO_STRING(mps_shm_region->control.opcode));
		    intflag = FALSE;
		    if (mps_shm_region->control.opcode == OPCODE_PLAY) {
		      mps_shm_region->player_status.state=STATE_PLAYING;
		      
		      if (mps_shm_region->control.skip_method!=SKIP_CURRENT) {
			// break out of decode loop, since we're not just unpausing
			MPS_DBG_PRINT("Leaving inner while loop because it's a play and skip\n");
			skipFlag=mps_shm_region->control.skip_method;
			break;
		      }
		      else {
			MPS_DBG_PRINT("Staying in inner while loop, because it's just an unpause.\n");
		      }
		    }
		    else if (mps_shm_region->control.opcode == OPCODE_STOP) {
		      mps_shm_region->player_status.state=STATE_STOPPED;
		      // Reset to the beginning of playlist
		      mps_shm_region->player_status.current_playlist_entry_id
			= MPS_PLAYLIST_BEGINNING;
		      MPS_DBG_PRINT("Leaving inner while loop, because it's a stop.\n");
		      break;
		    }
		    else if (mps_shm_region->control.opcode == OPCODE_PAUSE) {
		      mps_shm_region->player_status.state = STATE_PAUSED;
		      MPS_DBG_PRINT("Staying in inner while loop, because it's a pause.\n");
		    }		      		      
	          }

		  if(mps_shm_region->player_status.state != STATE_PAUSED) {
			int meta;
			if(param.frame_number > -1)
			{
				debug1("frames left: %li", (long) frames_left);
				if(!frames_left) break;
			}
			if(!play_frame()) break;
			if(!param.quiet)
			{
#if 0
				meta = mpg123_meta_check(mh);
				if(meta & (MPG123_NEW_ID3|MPG123_NEW_ICY))
				{
					if(meta & MPG123_NEW_ID3) print_id3_tag(mh, param.long_id3, stderr);
					if(meta & MPG123_NEW_ICY) print_icy(mh, stderr);
				}
#endif
			}
			if(!fresh && param.verbose)
			{
				if(param.verbose > 1 || !(framenum & 0x7))	print_stat(mh,0,0);
			}
#ifdef HAVE_TERMIOS
			if(!param.term_ctrl) continue;
			else term_control(mh, ao);
#endif
		  }
		  else {
		    usleep(250000); /* so that PAUSE doesn't hog CPU */ 
		  }
		}

	if(param.verbose) print_stat(mh,0,xfermem_get_usedspace(buffermem)); 

	if(!param.quiet)
	{
		double secs;
		mpg123_position(mh, 0, 0, NULL, NULL, &secs, NULL);
		MPS_DBG_PRINT("\n[%d:%02d] Decoding of %s finished.\n", 
			      (int)(secs / 60), ((int)secs) % 60, fname);
	}
	else if(param.verbose) MPS_DBG_PRINT( "\n");

	mpg123_close(mh);

    } /* end of loop over input files */
	/* Ensure we played everything. */
	if(param.smooth && param.usebuffer)
	{
		buffer_resync();
	}
	/* Free up memory used by playlist */    
	if(!param.remote) free_playlist();

	safe_exit(0); /* That closes output and restores terminal, too. */
	return 0;
}

static void print_title(FILE *o)
{
        MPS_DBG_PRINT("High Performance MPEG 1.0/2.0/2.5 Audio Player for Layers 1, 2 and 3\n");
	MPS_DBG_PRINT("\tversion %s; written and copyright by Michael Hipp and others\n", 
		      PACKAGE_VERSION);
	MPS_DBG_PRINT("\tfree software (LGPL/GPL) without any warranty but with best wishes\n");
}

static void usage(int err)  /* print syntax & exit */
{
	FILE* o = stdout;
	if(err)
	{
		o = stderr; 
		fprintf(o, "You made some mistake in program usage... let me briefly remind you:\n\n");
	}
	print_title(o);
	fprintf(o,"\nusage: %s [option(s)]\n", prgName);
	fprintf(o,"supported options [defaults in brackets]:\n");
	fprintf(o,"   -v    increase verbosity level       -q    quiet (don't print title)\n");
	fprintf(o,"   -t    testmode (no output)           -s    write to stdout\n");
	fprintf(o,"   -w <filename> write Output as WAV file\n");
	fprintf(o,"   -k n  skip first n frames [0]        -n n  decode only n frames [all]\n");
	fprintf(o,"   -c    check range violations         -y    DISABLE resync on errors\n");
	fprintf(o,"   -b n  output buffer: n Kbytes [0]    -f n  change scalefactor [%li]\n", param.outscale);
	fprintf(o,"   -r n  set/force samplerate [auto]\n");
	fprintf(o,"   -os,-ol,-oh  output to built-in speaker,line-out connector,headphones\n");
	#ifdef NAS
	fprintf(o,"                                        -a d  set NAS server\n");
	#elif defined(SGI)
	fprintf(o,"                                        -a [1..4] set RAD device\n");
	#else
	fprintf(o,"                                        -a d  set audio device\n");
	#endif
	fprintf(o,"   -2    downsample 1:2 (22 kHz)        -4    downsample 1:4 (11 kHz)\n");
	fprintf(o,"   -d n  play every n'th frame only     -h n  play every frame n times\n");
	fprintf(o,"   -0    decode channel 0 (left) only   -1    decode channel 1 (right) only\n");
	fprintf(o,"   -m    mix both channels (mono)       -p p  use HTTP proxy p [$HTTP_PROXY]\n");
	#ifdef HAVE_SCHED_SETSCHEDULER
	fprintf(o,"   -@ f  read filenames/URLs from f     -T get realtime priority\n");
	#else
	fprintf(o,"   -@ f  read filenames/URLs from f\n");
	#endif
	fprintf(o,"   -z    shuffle play (with wildcards)  -Z    random play\n");
	fprintf(o,"   -u a  HTTP authentication string     -E f  Equalizer, data from file\n");
	fprintf(o,"   -C    enable control keys            --no-gapless  not skip junk/padding in mp3s\n");
	fprintf(o,"   -?    this help                      --version  print name + version\n");
	fprintf(o,"See the manpage %s(1) or call %s with --longhelp for more parameters and information.\n", prgName,prgName);
	safe_exit(err);
}

static void want_usage(char* arg)
{
	usage(0);
}

static void long_usage(int err)
{
	char *enclist;
	FILE* o = stdout;
	if(err)
	{
  	o = stderr; 
  	fprintf(o, "You made some mistake in program usage... let me remind you:\n\n");
	}
	print_title(o);
	fprintf(o,"\nusage: %s [option(s)] [file(s) | URL(s) | -]\n", prgName);

	fprintf(o,"\ninput options\n\n");
	fprintf(o," -k <n> --skip <n>         skip n frames at beginning\n");
	fprintf(o," -n     --frames <n>       play only <n> frames of every stream\n");
	fprintf(o,"        --fuzzy            Enable fuzzy seeks (guessing byte offsets or using approximate seek points from Xing TOC)\n");
	fprintf(o," -y     --no-resync        DISABLES resync on error (--resync is deprecated)\n");
	fprintf(o," -p <f> --proxy <f>        set WWW proxy\n");
	fprintf(o," -u     --auth             set auth values for HTTP access\n");
	fprintf(o,"        --ignore-mime      ignore HTTP MIME types (content-type)\n");
	fprintf(o," -@ <f> --list <f>         play songs in playlist <f> (plain list, m3u, pls (shoutcast))\n");
	fprintf(o," -l <n> --listentry <n>    play nth title in playlist; show whole playlist for n < 0\n");
	fprintf(o,"        --loop <n>         loop track(s) <n> times, < 0 means infinite loop (not with --random!)\n");
	fprintf(o,"        --keep-open        (--remote mode only) keep loaded file open after reaching end\n");
	fprintf(o,"        --timeout <n>      Timeout in seconds before declaring a stream dead (if <= 0, wait forever)\n");
	fprintf(o," -z     --shuffle          shuffle song-list before playing\n");
	fprintf(o," -Z     --random           full random play\n");
	fprintf(o,"        --no-icy-meta      Do not accept ICY meta data\n");
	fprintf(o," -i     --index            index / scan through the track before playback\n");
	fprintf(o,"        --index-size <n>   change size of frame index\n");
	fprintf(o,"        --preframes  <n>   number of frames to decode in advance after seeking (to keep layer 3 bit reservoir happy)\n");
	fprintf(o,"        --resync-limit <n> Set number of bytes to search for valid MPEG data; <0 means search whole stream.\n");
	fprintf(o,"\noutput/processing options\n\n");
	fprintf(o," -o <o> --output <o>       select audio output module\n");
	fprintf(o,"        --list-modules     list the available modules\n");
	fprintf(o," -a <d> --audiodevice <d>  select audio device\n");
	fprintf(o," -s     --stdout           write raw audio to stdout (host native format)\n");
	fprintf(o," -S     --STDOUT           play AND output stream (not implemented yet)\n");
	fprintf(o," -w <f> --wav <f>          write samples as WAV file in <f> (- is stdout)\n");
	fprintf(o,"        --au <f>           write samples as Sun AU file in <f> (- is stdout)\n");
	fprintf(o,"        --cdr <f>          write samples as raw CD audio file in <f> (- is stdout)\n");
	fprintf(o,"        --reopen           force close/open on audiodevice\n");
	#ifdef OPT_MULTI
	fprintf(o,"        --cpu <string>     set cpu optimization\n");
	fprintf(o,"        --test-cpu         list optmizations possible with cpu and exit\n");
	fprintf(o,"        --list-cpu         list builtin optimizations and exit\n");
	#endif
	#ifdef OPT_3DNOW
	fprintf(o,"        --test-3dnow       display result of 3DNow! autodetect and exit (obsoleted by --cpu)\n");
	fprintf(o,"        --force-3dnow      force use of 3DNow! optimized routine (obsoleted by --test-cpu)\n");
	fprintf(o,"        --no-3dnow         force use of floating-pointer routine (obsoleted by --cpu)\n");
	#endif
	fprintf(o," -g     --gain             [DEPRECATED] set audio hardware output gain\n");
	fprintf(o," -f <n> --scale <n>        scale output samples (soft gain - based on 32768), default=%li)\n", param.outscale);
	fprintf(o,"        --rva-mix,\n");
	fprintf(o,"        --rva-radio        use RVA2/ReplayGain values for mix/radio mode\n");
	fprintf(o,"        --rva-album,\n");
	fprintf(o,"        --rva-audiophile   use RVA2/ReplayGain values for album/audiophile mode\n");
	fprintf(o," -0     --left --single0   play only left channel\n");
	fprintf(o," -1     --right --single1  play only right channel\n");
	fprintf(o," -m     --mono --mix       mix stereo to mono\n");
	fprintf(o,"        --stereo           duplicate mono channel\n");
	fprintf(o," -r     --rate             force a specific audio output rate\n");
	fprintf(o," -2     --2to1             2:1 downsampling\n");
	fprintf(o," -4     --4to1             4:1 downsampling\n");
  fprintf(o,"        --pitch <value>    set hardware pitch (speedup/down, 0 is neutral; 0.05 is 5%%)\n");
	fprintf(o,"        --8bit             force 8 bit output\n");
	fprintf(o,"        --float            force floating point output (internal precision)\n");
	audio_enclist(&enclist);
	fprintf(o," -e <c> --encoding <c>     force a specific encoding c (%s)\n", enclist != NULL ? enclist : "OOM!");
	fprintf(o," -d n   --doublespeed n    play only every nth frame\n");
	fprintf(o," -h n   --halfspeed   n    play every frame n times\n");
	fprintf(o,"        --equalizer        exp.: scales freq. bands acrd. to 'equalizer.dat'\n");
	fprintf(o,"        --gapless          remove padding/junk on mp3s (best with Lame tag)\n");
	fprintf(o,"                           This is on by default when libmpg123 supports it.\n");
	fprintf(o,"        --no-gapless       disable gapless mode, not remove padding/junk\n");
	fprintf(o," -D n   --delay n          insert a delay of n seconds before each track\n");
	fprintf(o," -o h   --headphones       (aix/hp/sun) output on headphones\n");
	fprintf(o," -o s   --speaker          (aix/hp/sun) output on speaker\n");
	fprintf(o," -o l   --lineout          (aix/hp/sun) output to lineout\n");

	fprintf(o,"\nmisc options\n\n");
	fprintf(o," -t     --test             only decode, no output (benchmark)\n");
	fprintf(o," -c     --check            count and display clipped samples\n");
	fprintf(o," -v[*]  --verbose          increase verboselevel\n");
	fprintf(o," -q     --quiet            quiet mode\n");
	#ifdef HAVE_TERMIOS
	fprintf(o," -C     --control          enable terminal control keys\n");
	#endif
	#ifndef GENERIG
	fprintf(o,"        --title            set xterm/rxvt title to filename\n");
	#endif
	fprintf(o,"        --long-tag         spacy id3 display with every item on a separate line\n");
	fprintf(o,"        --utf8             Regardless of environment, print metadata in UTF-8.\n");
	fprintf(o," -R     --remote           generic remote interface\n");
	fprintf(o,"        --remote-err       force use of stderr for generic remote interface\n");
	fprintf(o,"        --fifo <path>      open a FIFO at <path> for commands instead of stdin\n");
	#ifdef HAVE_SETPRIORITY
	fprintf(o,"        --aggressive       tries to get higher priority (nice)\n");
	#endif
	#if defined (HAVE_SCHED_SETSCHEDULER)
	fprintf(o," -T     --realtime         tries to get realtime priority\n");
	#endif
	fprintf(o," -?     --help             give compact help\n");
	fprintf(o,"        --longhelp         give this long help listing\n");
	fprintf(o,"        --version          give name / version string\n");

	fprintf(o,"\nSee the manpage %s(1) for more information.\n", prgName);
	safe_exit(err);
}

static void want_long_usage(char* arg)
{
	long_usage(0);
}

static void give_version(char* arg)
{
	fprintf(stdout, PACKAGE_NAME" "PACKAGE_VERSION"\n");
	safe_exit(0);
}
