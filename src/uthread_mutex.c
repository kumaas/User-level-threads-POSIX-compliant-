#include <stdio.h>
#include <stdlib.h>

#define MX_LOCKED 1
#define MX_UNLOCKED 0

#include "uthread.h"

/*
  The uthread_mutex_init() function is analogous to
  uthread_mutex_init with the default parameters enforced.
  There is no need to create a static initializer analogous to
  PTHREAD_MUTEX_INITIALIZER.
 */
int uthread_mutex_init(uthread_mutex_t* mutex){
  block_signals();
  steque_init(&mutex->waiting_threads);
  mutex->status = MX_UNLOCKED;
  mutex->curr_tid = -1;
  //printf("Mutex init done\n");
  unblock_signals();
  return 0;
}


/*
  The uthread_mutex_lock() is analogous to uthread_mutex_lock.
  Returns zero on success.
 */
int uthread_mutex_lock(uthread_mutex_t* mutex){
  block_signals();
  if (mutex->status == MX_UNLOCKED) {
    mutex->status = MX_LOCKED;
    mutex->curr_tid = currentThread->id;
    unblock_signals();
    return 0;
  }

  steque_enqueue(&mutex->waiting_threads, &currentThread->id);
  unblock_signals();

  while(1) {//no timeout, blocking call
    while (mutex->status == MX_LOCKED) {
      uthread_yield();
    }

    block_signals();
    if(mutex->status == MX_UNLOCKED){
        
        int * nextId = (int *)steque_front(&mutex->waiting_threads);
        if (*nextId == currentThread->id) {
          mutex->status = MX_LOCKED;
          mutex->curr_tid = currentThread->id;
          steque_pop(&mutex->waiting_threads);
          unblock_signals();
          return 0;
        } 
    }
    unblock_signals();
  }

  return 0;
}

/*
  The uthread_mutex_unlock() is analogous to uthread_mutex_unlock.
  Returns zero on success.
 */
int uthread_mutex_unlock(uthread_mutex_t *mutex){
  block_signals();
  mutex->curr_tid = -1;
  mutex->status = MX_UNLOCKED;
  unblock_signals();
  return 0;
}

/*
  The uthread_mutex_destroy() function is analogous to
  uthread_mutex_destroy and frees any resourcs associated with the mutex.
*/
int uthread_mutex_destroy(uthread_mutex_t *mutex){
  block_signals();
  steque_destroy(&mutex->waiting_threads);
  unblock_signals();
  return 0;
}

