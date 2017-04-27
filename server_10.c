/*
Projeto da Unidade Curricular:
Sistemas Operativos, Ano Letivo 16/17
Escola: DEI da UC
Alunos: 
    Gilberto Rouxinol - 2014230212
	Ricardo Cruz - 1995110956

Parte do código foi fornecida pelos docentes e outra parte foi desenvolvida pelos alunos e 
com o apoio da seguinte bibliografia:
(1) Apontamentos e exercícios das aulas TP e PL
(2) UnixTM Systems Programming: 
Communication, Concurrency, and Threads, by 
Kay A. Robbins , Steven Robbins
*/

/*
gcc server_10.c conf.c -lpthread -D_REENTRANT -Wall -DDEBUG=1 -o v10
*/
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// Includes adicionados pelos alunos  
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/shm.h> // para shared memory
#include <sys/mman.h> // para MMF
#include <fcntl.h> // para PIPES
#include <time.h>
#include <pthread.h>
#include "conf.h"

// Produce debug information
//#define DEBUG	  	1	

// Header of HTTP reply to client 
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024

// Defines adicionados pelos alunos 
#define PIPE_NAME "pipe_consola"
#define BUFFER_SIZE 25//tamanho do buffer de pedidos  (NAO FUNCIONA COM 150 POR EXEMPLO????)
#define BUFFER_MEMORIA_SIZE 10 //tamanho do buffer de memoria patilhada
#define HEADER_3   "Buffer de pedidos sem capacidade\r\n"

int  fireup(int port);
void identify(int socket);
void get_request(int socket);
int  read_line(int socket, int n);
void send_header(int socket);
void send_page(int socket);
void execute_script(int socket);
void not_found(int socket);
void catch_ctrlc(int);
void cannot_execute(int socket);

// Protótipos adicionados pelos alunos
void execute_gz(int socket);
void *worker(void* idp);  //função que as numero_threads vao executar para satisfazer os pedidos
int init();
int pool_threads(int i_start, int nthr);
int statistics_manager(long id);
int sever_main_process(long id);
void create_buffer ();
void *listening_pipe(void *arg); // função que é chamada pela thread_listening_pipe
void* scheduler(void* id_t);
void apagar_no(); // função que apaga a posição do array 
void buffer_ordenado(int policy); // função que ordena o buffer segundo a política definida
void create_buffer_aux();
void create_buffer_t();
int funcao_tipo_pedido(); //função que dada a string de pedido (req_buf) devolve o tipo de pedido
int indchr(char *s, char ch);
char *strsuf(char *suf, char *s);
char *strsuf2(char *suf, char *s);
char *strpre(char *conf, char *s);
int stralwd(char *strconf, char *strreq);

char buf[SIZE_BUF];
char req_buf[MAX_CHAR];
char buf_tmp[SIZE_BUF];
int port,socket_conn,new_conn;

// Atributos adicionados pelos alunos
// Estrutura para o Statistics Manager - O Statistics Manager vai ser um array destas estruturas
typedef struct {	
	int tipo_pedido;  // 0-Normal(*.html);  1-Comprimido (*.gz)
	char nome_ficheiro[MAX_CHAR];
	clock_t hora_pedido;
	clock_t hora_servido;
	int numero_thread;
} Stati;

// Estrutura do buffer
typedef struct buf_pedidos{
	int tipo_pedido;
	char buf_file[MAX_CHAR];
	int buf_conn;
	clock_t hora_chegada_pedido; 
}BUF_PEDIDOS;

//Estrutura para a variavel que vai ter os valores da 
//estatistica global. Acedida pelo Gestor de Estatisticas 
//e pela HANDLER do SIGUSR1

typedef struct estatistica_final{
	double media_0; //Media em tempo do pedido tipo 0 (html)
	double media_1; //Media em tempo do pedido tipo 1 (gz)
	double total_pedidos_0;// em tenpo 0
	double total_pedidos_1;//em tempo 1
	int conta_0;
	int conta_1;
} ESTATISTICA_FINAL;

// variaveis globais cujos valores estão no ficheiro config.txt
int numero_threads; //nº de threads workers
int policy; // politica de escalonamento
char ficheiros_gz[MAX_CHAR];
Config variavel_config;


// variáveis, mutex's e semáforos para threads
pthread_t my_thread[100];  // POSSO METER ISTO DENTRO DA FUNCAO pool_threads?
int id_thread[100]; // não pode estar aqui; o que aqui tem de estar é id_thread[numero_threads]!!!!

// Sincronização para o Buffer - implementação do Prod/Cons -> depois colar isto no server.h
sem_t* buffer_MUTEX, *buffer_EMPTY, *buffer_FULL;
int buffer_write;
int buffer_mem_write; 
int buffer_mem_read;

//Outras
BUF_PEDIDOS* buffer;
BUF_PEDIDOS* buffer_aux;
BUF_PEDIDOS t;

// Estrutura para sincronização da memoria partilhada entre o server e o estatisticas
sem_t *buffer_mem_MUTEX, *buffer_mem_EMPTY, *buffer_mem_FULL;

int shmid1; //ID da memoria partilhada

Stati*  shared_mem; // ponteiro para a memória partilhada 

pid_t childpid;
pid_t mypid;
//Atributos para a variavel de condicao
int numero_novo_threads;
int j = 0;
pthread_cond_t  modif_Nthreads = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_cond_var = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t pthread_mutex = PTHREAD_MUTEX_INITIALIZER;

void handler(int sinal) {
	#if DEBUG
		printf("\n\n*********** RECEBI O SINAL !!!!! ************* com id = %d\n", sinal );
	#endif
	
	if (sinal == SIGINT)
	{ 
	  //MATAR THREADS
	  while (waitpid(0,NULL,WNOHANG)>0); //caso haja processos filhos "pendurados"
	  shmdt(shared_mem); // faz o detach da memória partilhada 1
	  shmctl(shmid1,IPC_RMID,0); // remove a memória partilhada 1
	  printf ("Recebi o sinal SIGINT. Vou terminar\n");
	  unlink(PIPE_NAME); // elimina o named pipe
	  exit(0); 
	}
}


//CONFIGURAÇÕES INICIAIS DO SERVIDOR 
int init() {
	#if DEBUG
		printf("╔══════════════════════════════════════════════════════════════════════════════════════╗\n");
    	printf("║              HTTP SERVER by Gilberto Rouxinol e Ricardo Cruz                         ║\n");
    	printf("╚══════════════════════════════════════════════════════════════════════════════════════╝\n");
    #endif
  	
  	//Leitura de config.txt e atribuição dos seus valores nas variáveis globais
  	configuration(&variavel_config);
	port = variavel_config.cfg_serverport;
	numero_threads = variavel_config.cfg_threadpool;
	policy = variavel_config.cfg_scheduling;
    strcpy(ficheiros_gz,variavel_config.cfg_allowed);
	
	#if DEBUG
		printf ("<init>\nConfigurações iniciais do servidor:\n");
		printf("Porto:                     %d\n", port);
		printf("Número de threads:         %d\n", numero_threads);
		printf("Politica de escalonamento: %d\n", policy);
		printf("Ficheiros admissíveis:     %s\n", ficheiros_gz);
	#endif

	//Criação da memória partilhada para a escrita e leitura entre o Processo Statitics Managers e o Processo Server
    if( (shmid1 = shmget(IPC_PRIVATE, sizeof(Stati)*BUFFER_MEMORIA_SIZE, IPC_CREAT|0700)) < 0) {
        perror("Erro em shmget com IPC_CREAT 1\n");
        exit(1);
    }
	if((shared_mem = (Stati*) shmat(shmid1, NULL, 0)) == (Stati*)-1) {
		perror("Erro em shmat 1");
		exit(1);
	}


	//Criação dos mecanismos de sincronização entre o Processo Statitics Managers e o Processo Server
	sem_unlink("BUFFER_MEM_MUTEX");
	sem_unlink("BUFFER_MEM_EMPTY");
	sem_unlink("BUFFER_MEM_FULL" );
	buffer_mem_MUTEX = sem_open("BUFFER_MEM_MUTEX", O_CREAT|O_EXCL, 0700, 1);
	buffer_mem_EMPTY = sem_open("BUFFER_MEM_EMPTY", O_CREAT|O_EXCL, 0700, BUFFER_MEMORIA_SIZE);
	buffer_mem_FULL  = sem_open("BUFFER_MEM_FULL" , O_CREAT|O_EXCL, 0700, 0);
	
    
    //Criação de um named pipe
	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600) < 0) && (errno!= EEXIST)) {
		perror("Erro ao criar o PIPE!");
		exit(1);
	}

	return 0;
}

// Processo Gestor de Estatísticas
int statistics_manager(long id) {  
    #if DEBUG
    	printf("<statistics_manager()>\nProcesso Filho de PID %ld criado, cujo pai é de PID %ld.\n", (long) id, (long) getppid());
		printf("╔══════════════════════════════════════════════════════════════════════════════════════╗\n");
    	printf("║                                GESTOR de ESTATISTICAS                                ║\n");
    	printf("╚══════════════════════════════════════════════════════════════════════════════════════╝\n");
    #endif
    
    //Variáveis para as estatísticas agregadas (resultados)
    double total_ped_0 = 0; //em tempo
    double total_ped_1 = 0; //em tempo
	double med_0 = 0;
	double med_1 = 0;
    double tempo_servico;
    
    int cont_0 = 0;
    int cont_1 = 0;

	char teste0[MAX_CHAR];
	int teste1;
	clock_t teste2;
	clock_t teste3;

	buffer_mem_read = 0;
	
    while(1) {

        sem_wait(buffer_mem_FULL);
        sem_wait(buffer_mem_MUTEX);
        
		//sleep(2);
		//Leitura se houve zeracao da estatistica
		

		//TESTES//nome_ficheiro aparecer
        strcpy(teste0, shared_mem[buffer_mem_read].nome_ficheiro);
        teste1 = shared_mem[buffer_mem_read].tipo_pedido;
		teste2 = shared_mem[buffer_mem_read].hora_pedido;
		teste3 = shared_mem[buffer_mem_read].hora_servido;

		//tm_p2 = 
		localtime( &teste2);
    
		tempo_servico = teste3 - teste2;

		buffer_mem_read = (buffer_mem_read+1) % BUFFER_MEMORIA_SIZE;

		if(shared_mem[buffer_mem_read].tipo_pedido == 0) {
			total_ped_0 = total_ped_0 + tempo_servico;
			cont_0++;
		}else{
			total_ped_1 = total_ped_1 + tempo_servico;
			cont_1++;
		}

        printf("Statist Manager a falar:\nO pedido foi do tipo: %d\nNome do ficheiro: %s\nO pedido foi feito em clock: %f\nO pedido foi respondido em clock. %f\nTempo gasto a satisfazer o pedido: %f\n", teste1, teste0,(double) teste2, (double) teste3, tempo_servico);
        
        if(cont_0 != 0){
        	med_0 = total_ped_0/cont_0;
        	#if DEBUG
        		printf("Médias do tipo 0 (html): %f.\n", med_0);
        	#endif
        }
        if(cont_1 != 0){
        	med_1 = total_ped_1/cont_1;
        	#if DEBUG
        		printf("Médias do tipo 1 (gz): %f.\n", med_1);
        	#endif
        }
        #if DEBUG
    		printf("Statist Manager a dizer Adeus.\n");
    	#endif
        
        
        sem_post(buffer_mem_MUTEX);
        sem_post(buffer_mem_EMPTY);
    }
    return 0;
}

// Processo Server Main Process
int server_main_process(long id){
	
	pthread_t scheduler_thread; //thread responsavel pelo scheluling
	pthread_t listening_pipe_thread; // thread que está à escuta do PIPE
	int id_thread_scheduler; // ID da thread de escalonamento
	int id_thread_listening_pipe; // ID da thread que está à escuta do PIPE

	#if DEBUG
		printf("<server_main_process()>\nProcesso Pai de PID %ld criado.\n",id);	
	#endif
	//Criar o array buffer que guarda os pedidos
	create_buffer();  

	//Criar os mecanismos de sincronização para o buffer de pedidos
	sem_unlink("BUFFER_MUTEX");
	sem_unlink("BUFFER_EMPTY");
	sem_unlink("BUFFER_FULL");
	buffer_MUTEX = sem_open("BUFFER_MUTEX", O_CREAT|O_EXCL, 0700, 1);
    buffer_EMPTY = sem_open("BUFFER_EMPTY", O_CREAT|O_EXCL, 0700, BUFFER_SIZE); 
    buffer_FULL  = sem_open("BUFFER_FULL" , O_CREAT|O_EXCL, 0700, 0);
	
	//Criar a thread de escalonamento
	pthread_create(&scheduler_thread, NULL, scheduler, &id_thread_scheduler);

	//Ciar a thread que ficará à escuta do named pipe 
	pthread_create(&listening_pipe_thread, NULL, listening_pipe, &id_thread_listening_pipe);

	//Criar a pool de threads
	pool_threads(0, numero_threads);
	sleep(2);
    return 0;
}

//Tarefa de escalonamento dos pedidos - PRODUTOR
void* scheduler(void *id_t) {
	int socket_conn;
	clock_t now; //Hora de chegada do pedido
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	//Posicao de escrita no buffer
	buffer_write = 0;  

	//Configure listening port
	if ((socket_conn=fireup(port))==-1)
		exit(1);

	//Serve requests 
	while (1) 	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		// Identify new client
		identify(new_conn);

		now = clock();//time(NULL);

		// Process request e verifica se gz permitido
		get_request(new_conn); 

		if (buffer_write < BUFFER_SIZE){
			//Faz wait no MUTEX e no SEMAFORO EMPTY
			sem_wait (buffer_EMPTY);
			sem_wait (buffer_MUTEX);

			//Adiciona pedido ao buffer
			       buffer[buffer_write].tipo_pedido         = funcao_tipo_pedido();
			strcpy(buffer[buffer_write].buf_file,             req_buf);
		 	       buffer[buffer_write].buf_conn            = new_conn;
			       buffer[buffer_write].hora_chegada_pedido = now;

		   	#if DEBUG
		   		printf("<scheduler>\n");
		    	printf("Tipo de Pedido: %d\n",  buffer[buffer_write].tipo_pedido);
		    	printf("Página:         %s\n",  buffer[buffer_write].buf_file);
		    	printf("Socket:         %d\n",  buffer[buffer_write].buf_conn);
		    	printf("Hora chegada:   %ld\n", buffer[buffer_write].hora_chegada_pedido);
		    #endif

		    //Passa para a posicao seguinte
			buffer_write++;
			
			buffer_ordenado(policy);
			
			//faz post ao MUTEX e ao SEMAFORO FULL
			sem_post (buffer_MUTEX);
			sem_post (buffer_FULL);
		} else {
			 //Buffer cheio - declina pedido, não o colocando no buffer
			 printf("\nBuffer de pedidos sem capacidade de resposta neste momento.");
			 sprintf(buf,HEADER_3);
			 send(new_conn,buf,strlen(HEADER_3),0); 
		}
	}
	return NULL;
}

//Tarefas workers que satisfazem os pedidos - CONSUMIDORES
void *worker(void* id_ptr) {
	
	int id_thread = *((int *)id_ptr);
	char file[MAX_CHAR];
	int leitura_tipo_pedido, socket;
	clock_t hora_chegada_pedido;
	clock_t hora_tratamento_pedido;

	buffer_mem_write = 0;
	
	while (1){
		pthread_mutex_lock(&pthread_mutex);
		if(id_thread >= numero_threads){
			pthread_mutex_unlock(&pthread_mutex);
		    #if DEBUG
		     	printf("<worker>\nExit da thread n.º %d\n", id_thread+1);
			#endif
			pthread_exit(NULL);
		}	
		pthread_mutex_unlock(&pthread_mutex);
		
		
		//fazer wait ao MUTEX e SEMAFORO FULL do buffer
		sem_wait (buffer_FULL);
		sem_wait (buffer_MUTEX);

		//sleep(5);
		printf("<worker>\nSou a thread %d e vou servir um pedido.\n", id_thread);
		leitura_tipo_pedido = buffer[0].tipo_pedido;
		socket              = buffer[0].buf_conn;
		strcpy(file,          buffer[0].buf_file);
		hora_chegada_pedido = buffer[0].hora_chegada_pedido;
		
		//Com isto deixa de haver este warning!!
		printf("%d",socket);
		
		apagar_no();

		//fazer post ao MUTEX e SEMAFORO EMPTY do buffer
		sem_post(buffer_MUTEX);
		sem_post(buffer_EMPTY);

		//satisfação do pedido
		if ((leitura_tipo_pedido == 0)){
			send_page(new_conn);
		} else if ((leitura_tipo_pedido == 1)) {
			execute_gz(new_conn);
		} else {
			printf("Nao faco nada!");
		}
		
		close(new_conn);

		// escreve na memoria partilhada
		hora_tratamento_pedido = clock();//time(NULL);
		if (buffer_mem_write < BUFFER_MEMORIA_SIZE) {
			sem_wait(buffer_mem_EMPTY);
			sem_wait(buffer_mem_MUTEX);
			
			       shared_mem[buffer_mem_write].tipo_pedido  = leitura_tipo_pedido;
			strcpy(shared_mem[buffer_mem_write].nome_ficheiro, file);
			       shared_mem[buffer_mem_write].hora_pedido  = hora_chegada_pedido;
			       shared_mem[buffer_mem_write].hora_servido = hora_tratamento_pedido;

			buffer_mem_write = (buffer_mem_write + 1) % BUFFER_MEMORIA_SIZE;

			sem_post(buffer_mem_MUTEX);
			sem_post(buffer_mem_FULL);
		} else {
			printf("\n\n ENCHEU ARRAY ESTATISTICAS \n\n!!!");
		}
	}
}

void *listening_pipe(void *arg) {
	#if DEBUG
		printf("<listening_pipe>\nThread à escuta do PIPE ...\n");
	#endif

	int fd_pipe_read;
	int i, diff;
	char str_read[MAX_CHAR];
	char prefixo_str_read[MAX_CHAR];
	char suffixo_str_read[MAX_CHAR];
	char str_thr[MAX_CHAR];
	char str_sch[MAX_CHAR];
	char str_all[MAX_CHAR];

	strcpy(str_thr,"THREAD");
    strcpy(str_sch,"SCHEDULING");
    strcpy(str_all,"ALLOWED");

	if ((fd_pipe_read = open(PIPE_NAME, O_RDWR))< 0) {
		perror("Pipe não aberto para leitura");
		exit(0);
	}
	
	while(1){
		read( fd_pipe_read, str_read, sizeof(char)*MAX_CHAR );
		
		#if DEBUG
			printf("\nLEITURA DO PIPE: %s\n", str_read);
		#endif

		//Desagregacao da string lida:
		//Leitura do seu prefixo e sufixo 
		strpre(prefixo_str_read, str_read);
		strsuf2(suffixo_str_read, str_read);
		
		//Tratamento de string que recebe nova informacao sobre numero de threads 
		if(strcmp(prefixo_str_read,str_thr) == 0){
			numero_novo_threads = atoi(suffixo_str_read);
			diff = numero_threads - numero_novo_threads;
			pthread_mutex_lock(&pthread_mutex);
				numero_threads = numero_novo_threads;
				#if DEBUG
					printf("\n\n<listening_pipe>\nnumero_threads = %d\n\n",numero_threads);
				#endif
			pthread_mutex_unlock(&pthread_mutex);

			if(diff > 0){
				for( i = 0; i < diff; i++)
					#if DEBUG
						printf("\n<listening_pipe>\nJoint da thread n.º %d\n", numero_threads + i + 1);
					#endif
					pthread_join(my_thread[numero_threads + i],NULL);
			}else if(diff < 0){
				pool_threads(numero_threads+diff, numero_threads);
			}else{
				printf("O numero de threads manteve-se");
			continue;
			}
		}
		
		//Tratamento de string que recebe nova informacao sobre politica de escalonamento 
		if(strcmp(prefixo_str_read,str_sch) == 0){
			policy = atoi(suffixo_str_read);
		}

		//Tratamento de string que recebe nova informacao sobre novos gz admissiveis 
		if(strcmp(prefixo_str_read,str_all) == 0){
			strcpy(ficheiros_gz,suffixo_str_read);
		}
	}
}

//Ordenar o buffer de pedidos atendendo a politica de escalonamento
void buffer_ordenado(int policy)
{
   //Policy 0: FIFO (O Primeiro a pedir is Primeiro a ser servido)
   //Policy 1: serve primeiro os pedidos html
   //Policy 2: serve primeiro os pedidos gz
   int i, j, k, a, b;

   if(policy == 0){ //Ordenacao FIFO: Metodo de ordenacao por insercao - O(n) ou O(n^2)
   		//create_buffer_t();
   		for(i = 1; i < buffer_write; i++){
   			t.tipo_pedido = buffer[i].tipo_pedido;
            strcpy(t.buf_file, buffer[i].buf_file);
            t.buf_conn = buffer[i].buf_conn;
   			t.hora_chegada_pedido = buffer[i].hora_chegada_pedido;
   			for(j = i-1; j >= 0 && t.hora_chegada_pedido < buffer[j].hora_chegada_pedido; j--){
   				buffer[j+1].tipo_pedido = buffer[j].tipo_pedido;
            	strcpy(buffer[j+1].buf_file, buffer[j].buf_file);
            	buffer[j+1].buf_conn = buffer[j].buf_conn;
            	buffer[j+1].hora_chegada_pedido = buffer[j].hora_chegada_pedido;
   			}
   			buffer[j+1].tipo_pedido = buffer[j].tipo_pedido;
            strcpy(buffer[j+1].buf_file, buffer[j].buf_file);
            buffer[j+1].buf_conn = buffer[j].buf_conn;
            buffer[j+1].hora_chegada_pedido = buffer[j].hora_chegada_pedido;
   		}
   	}else{
		create_buffer_aux();
		k = 0;
		if(policy == 1){ //ordenacao html
			a = 0; //tipo pedido 0 -> html
			b = 1; //tipo pedido 1 -> gz
		}
		if(policy == 2){ //ordenacao gz
			a = 1; //tipo pedido 1 -> gz
			b = 0; //tipo pedido 0 -> html
		}
		//Coloca ordenadamente os tipo de pedidos prioritarios
    	for(i = 0; i < buffer_write; i++){
        	if(buffer[i].tipo_pedido == a){
            	buffer_aux[k].tipo_pedido = buffer[i].tipo_pedido;
            	strcpy(buffer_aux[k].buf_file, buffer[i].buf_file);
            	buffer_aux[k].buf_conn = buffer[i].buf_conn;
            	buffer_aux[k].hora_chegada_pedido = buffer[i].hora_chegada_pedido;
            	k++;
           	}
       	}
    	//Coloca ordenadamente os tipo de pedidos nao prioritarios
    	for(i = 0; i < buffer_write; i++){
        	if(buffer[i].tipo_pedido == b){
            	buffer_aux[k].tipo_pedido = buffer[i].tipo_pedido;
            	strcpy(buffer_aux[k].buf_file, buffer[i].buf_file);
            	buffer_aux[k].buf_conn = buffer[i].buf_conn;
            	buffer_aux[k].hora_chegada_pedido = buffer[i].hora_chegada_pedido;
            	k++;
        	}
   		}
   		//Devolve array buffer ordenado
   		for(i = 0; i < buffer_write; i++){
       		buffer[i].tipo_pedido = buffer_aux[i].tipo_pedido;
        	strcpy(buffer[i].buf_file, buffer_aux[i].buf_file);
        	buffer[i].buf_conn = buffer_aux[i].buf_conn;
        	buffer[i].hora_chegada_pedido = buffer_aux[i].hora_chegada_pedido;
   		}
		free(buffer_aux);
	}
}
//Aloca memoria para criar o array bufer que vai ficar com os pedidos
void create_buffer() {
	if ((buffer = (BUF_PEDIDOS *) malloc( sizeof(BUF_PEDIDOS)*BUFFER_SIZE) ) == NULL) {
		perror ("Erro ao alocar espaço em memória para o buffer de pedidos\n");
		exit(1);
	}
}

//Aloca memoria para criar o array bufer auxiliar que vai ficar com os pedidos
void create_buffer_aux() {
	if ((buffer_aux = (BUF_PEDIDOS *) malloc( sizeof(BUF_PEDIDOS)*BUFFER_SIZE) ) == NULL) {
		perror ("Erro ao alocar espaço em memória para o buffer auxiliar de pedidos\n");
		exit(1);
	}
}
//Cria um array de threads com a tarefa worker
int pool_threads(int i_start, int nthr){
	for(int i = i_start; i < nthr; i++){
  		id_thread[i] = i;
  		if(pthread_create(&my_thread[i], NULL, worker, &id_thread[i]) != 0){
  			perror("Erro ao criar a thread.");
  			exit(1);
  		}
  		#if DEBUG
  			printf("<pool_threads>\nCreate thread n.º %d\n",i+1);
  		#endif
  	}
	return 0;
}
//Liberta a posicao zero do buffer apos servir pedido
//e ripa os outros todos para tras de uma posicao
void apagar_no() {	
	for (int i = 1; i < buffer_write; i++){
		       buffer[i-1].tipo_pedido         = buffer[i].tipo_pedido;
		strcpy(buffer[i-1].buf_file,             buffer[i].buf_file);
		       buffer[i-1].buf_conn            = buffer[i].buf_conn;
		       buffer[i-1].hora_chegada_pedido = buffer[i].hora_chegada_pedido;
	}
	buffer_write--;
}

//Identifica o tipo de pedido
int funcao_tipo_pedido() {
	char suf[8];
	if(strcmp(strsuf(suf,req_buf), "html") == 0){ //html
        return 0;
    } else if(strcmp(strsuf(suf,req_buf), "gz") == 0){ //gz
        return 1;
    } else {
        return -1;
    }
}
//Procura posicao ou indice na string onde esta o carater ponto
int indchr(char *s, char ch){
	int i;
	for(i=0; s[i]!='\0'; i++)
		if(s[i] == ch)
			return i;
	return -1;
}
//Copia prefixo antes do carater dois pontos para nova string
char *strpre(char *conf, char *s){
  int i;
  i = 0;
  while(s[i]!=':'){
      conf[i] = s[i];
      i++;
  }
  conf[i] = '\0';
  return conf;
}
//Nota: Agregar estas duas funcoes: strsuf() e strsuf2() numa unica,
//caso haja tempo depois!!!!
//Copia sufixo a seguir ao carater ponto para nova string
char *strsuf(char *suf, char *s){
	int indice;
	int i = 0;
	indice = indchr(s,'.') + 1;
	while((suf[i++] = s[indice++]))
		;
	return suf;
}
//Copia sufixo a seguir ao carater dois pontos para nova string
char *strsuf2(char *suf, char *s){
  int indice;
  int i = 0;
  indice = indchr(s,':') + 1;
  while((suf[i++] = s[indice++]))
    ;
  return suf;
}
//Verifica permissao de ficheiro gz 
int stralwd(char *strconf, char *strreq){
	char *s;
	s = strstr(strconf,strreq);
	if(s != NULL){
		return 1;//permitido
	}else{
		return -1;//nao permitido
	}
}
// Processes request from client
void get_request(int socket)
{
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}	

	// Currently only supports GET 
	if(!found_get) {
		printf("<get_request>\nRequest from client without a GET\n");
		exit(1);
	}
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"%s","index.html");
	// Se a pagina nao is permitida entao mostra uma pagina a informar
	// para tal usa-se htdocs/naopermitido.html
	if( (funcao_tipo_pedido() == 1) && (stralwd(ficheiros_gz, req_buf) == -1))
		sprintf(req_buf,"%s","naopermitido.html");

	#if DEBUG
		printf("<get_request>\nClient requested the following page: %s\n",req_buf);
	#endif

	return;
}


// Send message header (before html page) to client
void send_header(int socket)
{
	#if DEBUG
		printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buf,HEADER_1);
	send(socket,buf,strlen(HEADER_1),0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf,strlen(SERVER_STRING),0);
	sprintf(buf,HEADER_2);
	send(socket,buf,strlen(HEADER_2),0);

	return;
}


// Funcao para tratamento do pedido GZ  @v1
void execute_gz(int socket)  //esta funcao tem de receber o nome do ficheiro.gz certo????
{
	int fd[2];
	pipe(fd);
	char req_buf_zg[MAX_CHAR];
	char caminho[4] ="gz/";
	
	sprintf(req_buf_zg,"%s",strcat(caminho,req_buf));
	
	#if DEBUG
		printf("req_buf_zg = %s\n", req_buf_zg);
	#endif

	if (fork() == 0) {
    	//re-direciona o stdout para o input do PIPE e fecha os fd's que nao são necessários
    	dup2(fd[1], fileno(stdout));
    	close(fd[0]);
    	close(fd[1]);
    	execlp("gzip", "gzip" , "-k", "-d", "-c", req_buf_zg, NULL);  //ALTERAR o NOME DO FILE!!!
    	exit(0);
	}else{
  		wait(NULL);
  		int total = 0;
  		char str[512];
  		total = read(fd[0], str , sizeof(str));
  		str[total-1] ='\0';
  		//printf("%s %d\n", str, total);
  		//enviar ao cliente - ver se sao necessárias seguintes linhas comentadas
  		// First send HTTP header back to client
  		send_header(socket);
  		send(socket,str,strlen(str),0);
	}
	return;
}


// Send html page to client
void send_page(int socket)
{
	FILE * fp;

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",req_buf);

	#if DEBUG
		printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else {
		// Page found, send to client 
	
		// First send HTTP header back to client
		send_header(socket);

		//printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,SIZE_BUF,fp))
			send(socket,buf_tmp,strlen(buf_tmp),0);
		
		// Close file
		fclose(fp);
	}

	return; 

}


// Identifies client (address and port) from socket
void identify(int socket)
{
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	//int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	//printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
}


// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n) 
{ 
	int n_read;
	int not_eol; 
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}		
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
		printf("read_line: new line read from client socket: %s\n",buf);
	#endif
	
	return n_read;
}


// Creates, prepares and returns new socket
int fireup(int port)
{
	int new_sock;
	struct sockaddr_in name;

	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port 
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}
 
	return(new_sock);
}

// Sends a 404 not found status message to client (page not found)
void not_found(int socket)
{
 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}

// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket)
{
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}

// Closes socket before closing
void catch_ctrlc(int sig)
{
	printf("Server terminating\n");
	close(socket_conn);
	exit(0);
}

int main(int argc, char ** argv) {
	
	for (int j =1; j < 32; j++) {
		signal(j, handler);
	}
	
	//Limpa ecrã:
	system("clear"); 

	//Inicialização:
	if(init() != 0){
        perror("Failed to init()");
        exit(1);
    }
    
    //Criaçao dos processos:
    mypid = getpid();
    childpid = fork();
    
    if (childpid == -1) {
    	perror("Failed to fork()");
    	return 1;
    }
      
    if (childpid == 0) { 
    	statistics_manager(getpid()); //Processo filho - Gestão de Estatisticas 
        exit(0);
    }
    else {
    	server_main_process(getpid()); //Processo pai - Servidor
    	wait(NULL);
    } 

    return 0;
}