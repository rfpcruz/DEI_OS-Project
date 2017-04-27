#ifndef CONF_H_INCLUDED
#define CONF_H_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MAX_CHAR 1024

typedef struct config{
	int cfg_serverport;
	int cfg_threadpool;
	int cfg_scheduling;
	char cfg_allowed[MAX_CHAR];
} Config;

void configuration(Config* );
#endif
