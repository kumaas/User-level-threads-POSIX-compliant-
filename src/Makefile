CC = gcc
CPPFLAGS = -g -Wall
AR=ar
ARFLAGS = -rcs
RANLIB  = ranlib
RM = rm

LIB = uthread.a
SRC = uthread_sched.c uthread_mutex.c steque.c
OBJ = $(patsubst %.c,%.o,$(SRC))

all : $(LIB)

$(LIB) : $(OBJ)
	ar $(ARFLAGS) $@ $<
	$(RANLIB) $@

%.o : %.c
	$(CC) -c $< $(CPPFLAGS)

clean :
	$(RM) -f $(LIB) $(OBJ)
