LIBS=-lrt
CFLAGS=-g
SRC=rt_task.c dl_syscalls.c
OBJ=rt_task.o dl_syscalls.o
rt_task: ${OBJ}
	gcc ${CFLAGS} ${OBJ} ${LIBS} -o $@ 

dl_syscalls.o:	dl_syscalls.h dl_syscalls.c
	gcc ${CFLAGS} -c dl_syscalls.c -o $@
rt_task.o:	rt_task.h rt_task.c
	gcc ${CFLAGS} -c rt_task.c -o $@
clean:
	rm -rf *.o rt_task
install:
	cp rt_task /usr/bin
uninstall:
	rm  /usr/bin/rt_task
