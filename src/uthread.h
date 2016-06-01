#ifndef UTHREAD_H
#define UTHREAD_H

#include "steque.h"
#include <ucontext.h>

typedef enum {ST_ALIVE, ST_FINISHED, ST_CANCELLED} ThreadStatus;

/* Define uthread_t and uthread_mutex_t types here */
typedef struct uthread_t {
  int id;
  ucontext_t uc;
  ThreadStatus status;
  void* retval;
  int joined_tid;
} uthread_t;

typedef struct uthread_mutex_t {
  steque_t waiting_threads;
  int curr_tid;
  int status;
} uthread_mutex_t;

// Current running thread!
extern uthread_t * currentThread;

void uthread_init(long period);
int  uthread_create(uthread_t *thread,
                     void *(*start_routine)(void *),
                     void *arg);
int  uthread_join(uthread_t thread, void **status);
void uthread_exit(void *retval);
void uthread_yield(void);
int  uthread_equal(uthread_t t1, uthread_t t2);
int  uthread_cancel(uthread_t thread);
uthread_t uthread_self(void);

void block_signals(void);
void unblock_signals(void);

int  uthread_mutex_init(uthread_mutex_t *mutex);
int  uthread_mutex_lock(uthread_mutex_t *mutex);
int  uthread_mutex_unlock(uthread_mutex_t *mutex);
int  uthread_mutex_destroy(uthread_mutex_t *mutex);
#endif

