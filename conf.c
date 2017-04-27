#include "conf.h"

void configuration(CONFIG *cfg){
	FILE* cf_fp;
	char line[MAX_CHAR];

	cf_fp = fopen("config.txt","r");

	if(cf_fp == NULL){
		perror("File 'config.txt' doesnt exist");
	}
	
	//SERVERPORT
	fgets(line,MAX_CHAR,cf_fp);
	(*cfg).cfg_serverport = atoi(line);
	
	//THREADPOOL
	fgets(line,MAX_CHAR,cf_fp);
	cfg->cfg_threadpool = atoi(line);
	
	//THREADPOOL
	fgets(line,MAX_CHAR,cf_fp);
	cfg->cfg_scheduling = atoi(line);
	
	//ALLOWED
	fgets(line,MAX_CHAR,cf_fp);
	strcpy(cfg->cfg_allowed,line);
	
	#ifdef DEBUG
		printf("(*cfg).cfg_serverport = %d\n",(*cfg).cfg_serverport);
		printf("cfg->cfg_threadpool = %d\n",cfg->cfg_threadpool);
		printf("cfg->cfg_scheduling = %d\n",cfg->cfg_scheduling);
		printf("cfg->cfg_allowed = %s\n",cfg->cfg_allowed);
	#endif
	
	fclose(cf_fp);
}
