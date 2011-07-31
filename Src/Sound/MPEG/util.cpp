/* this file is a part of amp software

	util.c: created by Andrew Richards

*/

#define AMP_UTIL
#include "amp.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "audio.h"

struct debugFlags_t debugFlags;


/* die - for terminal conditions prints the error message and exits */
/* can not be suppressed with -q,-quiet	*/
void
die(char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
}


/* warn - for warning messages. Can be suppressed by -q,-quiet			*/
void
warn(char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
	if (!A_QUIET) {
		fprintf(stderr,"Warning: ");
		vfprintf(stderr, fmt, ap);
	}
}


/* msg - for general output. Can be suppressed by -q,-quiet. Output */
/* goes to stderr so it doesn't conflict with stdout output */
void
msg(char *fmt, ...)
{
	va_list ap;
	va_start(ap,fmt);
 
	if (!A_QUIET)
	{
	  if (A_MSG_STDOUT) {
	    vfprintf(stdout, fmt, ap);
	    fflush(stdout);
	  } else {
	    vfprintf(stderr, fmt, ap);
	    fflush(stderr);
	  }
	}
}

void
debugOptions()
{
	int idx;
	msg("Possible options are: ");
	for(idx=0;debugLookup[idx].name!=0;idx++)
		msg("%s,",debugLookup[idx].name);
	msg("\010 \n");
}


/* debugSetup - if debugging is turned on sets up the debug flags from */
/* the command line arguments */

void
debugSetup(char *dbgFlags)
{
#ifndef DEBUG
	warn("Debugging has not been compiled into this version of amp\n");
#else
	char *ptr;
	int idx;

	memset(&debugFlags,0,sizeof(debugFlags));

	ptr=strtok(dbgFlags,",");
	while(ptr) {
		for(idx=0;debugLookup[idx].name!=0;idx++) {
			if (strcmp(debugLookup[idx].name,ptr)==0) {
				*(debugLookup[idx].var)=1;
				break;
			}
		}
		if (debugLookup[idx].name==0) {
			warn("Debug option, %s, does not exist\n",ptr);
			debugOptions();
			exit(1);
		}
		ptr=strtok(NULL,",");
	}

	DB(args,
		 for(idx=0;debugLookup[idx].name!=0;idx++) 
			 printf("Flag: %s = %d\n",debugLookup[idx].name,*(debugLookup[idx].var));
		 );
#endif
}

