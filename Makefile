FLAGS  = -lpthread -D_REENTRANT -Wall -DDEBUG=1
CC     = gcc
PROG   = server_12
OBJS   = conf.o server_12.o

all:	${PROG}

clean:
	rm ${OBJS} ${EXE} *~
  
${PROG}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} $< -c

##########################

conf.o:  conf.h conf.c

server_12.o: conf.h server_12.c

server_12: server_12.o conf.o

