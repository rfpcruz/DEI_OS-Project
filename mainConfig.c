//gcc mainConfig.c -o mainConfig
//gcc -c mainConfig.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAX_CHAR 1024
#define PIPE_THREAD	"THREAD:"
#define PIPE_SCHEDU	"SCHEDULING:"
#define PIPE_ALLOWE	"ALLOWED:"
#define PIPE_NAME "pipe_consola"

void clrscr(){
    system("@cls||clear");
}

int main (){
    FILE* cf_fp;
	  char line[MAX_CHAR];
	  char string_opcao[MAX_CHAR];
	  int i;
    int opcao;
  	
    int numero_threads;
    int numero_novo_threads;
  	
    int policy;
    int novo_policy;
  	
    char str_allowed[MAX_CHAR];
    char str_novo_allowed[MAX_CHAR];
  	
    char string_cfg_threadpool[MAX_CHAR];
    char string_cfg_scheduling[MAX_CHAR];
    char string_cfg_allowed[MAX_CHAR];

    int fd;

    strcpy(string_cfg_threadpool, "THREAD: nao atualizado");
    strcpy(string_cfg_scheduling, "SCHEDULING: nao atualizado");
    strcpy(string_cfg_allowed, "ALLOWED: nao atualizado");

    cf_fp = fopen("config.txt","r");
	  if(cf_fp == NULL){
		  perror("File 'config.txt' doesnt exist");
		  exit(1);
	  }
	  fgets(line,MAX_CHAR,cf_fp);
	
	  fgets(line,MAX_CHAR,cf_fp);
	  numero_threads = atoi(line);
	
	  fgets(line,MAX_CHAR,cf_fp);
	  policy = atoi(line);
	
	  fgets(line,MAX_CHAR,cf_fp);
	  strcpy(str_allowed, line); 

	  fclose(cf_fp);

    if ((fd = open(PIPE_NAME, O_WRONLY)) < 0){
        perror("Sem possibilidade de abrir o pipe para escrita.");
        exit(1);
    } 

  	while(1){
        printf("\nNova configuracao:\n\
           1 - Alterar o numero de threads\n\
           2 - Estabelecer uma nova politica de escalonamento\n\
           3 - Atualizar os ficheiros gz admissiveis\n\
           4 - Sair da configuracao\a\n");
        i = scanf("%d",&opcao);
        if (i == 0 || opcao <= 0 || opcao > 4) {
            scanf("%s",string_opcao);
            clrscr();
            printf("Escolha uma das opcoes disponiveis.\n");
            continue;
        }
        switch(opcao){
            case 1:printf("Numero de threads atuais: %d\n",numero_threads);
            		   do{
        				      printf("Novo numero de threads:\n");
        			 	      scanf("%d", &numero_novo_threads);
    				       }while(numero_novo_threads < 1 || numero_novo_threads > 15);
    				       getchar();
                   sprintf(string_cfg_threadpool,"%s %d",PIPE_THREAD,numero_novo_threads);
                   write(fd, string_cfg_threadpool, sizeof(char)*MAX_CHAR);
                   break;
            case 2:printf("Politica de escalonamento atual: %d\n",policy);
            		   do{
        				      printf("Nova politica de escalonamento:\n");
        				      scanf("%d", &novo_policy);
    				       }while(novo_policy < 0 || novo_policy > 2);
    				       getchar();
                   sprintf(string_cfg_scheduling,"%s %d",PIPE_SCHEDU,novo_policy);
                   write(fd, string_cfg_scheduling, sizeof(char)*MAX_CHAR);
                   break;
            case 3:printf("Ficheiros gz admissiveis atualmente: %s\n",str_allowed);
        			     printf("Indique novamente todos os ficheiros que pretende tornar admissiveis,\nseparados por virgula:\n");
        			     getchar();
                   fgets(str_novo_allowed,MAX_CHAR,stdin);
                   str_novo_allowed[strlen(str_novo_allowed)-1] = '\0';
                   sprintf(string_cfg_allowed,"%s %s",PIPE_ALLOWE,str_novo_allowed);
                   write(fd, string_cfg_allowed, sizeof(char)*MAX_CHAR);
                   break;
            case 4:printf("Configuracao depois da atualizacao:\n");
                   printf("   %s\n",string_cfg_threadpool);
                   printf("   %s\n",string_cfg_scheduling);
                   printf("   %s\n",string_cfg_allowed);
                   printf("A sair ...\n");
                   return 0;
        }
    }
  return 0;
}