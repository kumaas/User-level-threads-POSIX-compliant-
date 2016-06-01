#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "uthread.h"

#define UT_MAX_N_THREADS 50
#define UTHREAD_CANCELLED 13

static int* uthread_cancel_retval;

static sigset_t ut_sigset;  

static steque_t ut_queue;
static uthread_mutex_t queueMutex;

static uthread_t* threads[UT_MAX_N_THREADS];

static uthread_t mainThread;

uthread_t* currentThread;
uthread_mutex_t currentThreadMutex;

static int idCounter = 0;
static uthread_mutex_t idMutex;

static void scheduler(int signum) {
  //no mutex used in this func for simplicity, simply interuupts disabled till return instruction
  block_signals();

  if (steque_isempty(&ut_queue)) {
    // Current thread finished and no more threads to run.
    if ((currentThread->status == ST_FINISHED) || (currentThread->status == ST_CANCELLED)) {
      // Swap current thread back to main thread which used uthread_exit and so is not in the queue
      ucontext_t * curContext = &currentThread->uc;
      currentThread = &mainThread;
      unblock_signals();
      swapcontext(curContext, &mainThread.uc);
      return;
    }
    // No other thread to move on
    unblock_signals();
    return;
  }

  uthread_t* nextThread = steque_pop(&ut_queue);

  // Do not enqueue thread that already terminated.
  if ((currentThread->status != ST_FINISHED) && (currentThread->status != ST_CANCELLED)) {
    steque_enqueue(&ut_queue, currentThread);
  }
  ucontext_t * currentContext = &currentThread->uc;
  currentThread = nextThread;
  unblock_signals();
  swapcontext(currentContext, &nextThread->uc);
}

static void run_thread(void *(*start_routine)(void *), void* args)
{
  void * retval = start_routine(args);
  uthread_mutex_lock(&currentThreadMutex);
  if (uthread_equal(*currentThread, mainThread)) {
    uthread_mutex_unlock(&currentThreadMutex);
    int * t = (int *)retval;
    exit(*t);
  } 
  uthread_mutex_unlock(&currentThreadMutex);
  uthread_exit(retval);
  
}

void block_signals(void){
    sigprocmask(SIG_BLOCK, &ut_sigset, NULL);
}

void unblock_signals(void){
    sigprocmask(SIG_UNBLOCK, &ut_sigset, NULL);
}

/*
  The uthread_init() function does not have a corresponding uthread equivalent.
  It must be called from the main thread before any other UThreads
  functions are called. It allows the caller to specify the scheduling
  period (quantum in micro second), and may also perform any other
  necessary initialization.  If period is zero, then thread switching should
  occur only on calls to uthread_yield().

  Recall that the initial thread of the program (i.e. the one running
  main() ) is a thread like any other. It should have a
  uthread_t that clients can retrieve by calling uthread_self()
  from the initial thread, and they should be able to specify it as an
  argument to other UThreads functions. The only difference in the
  initial thread is how it behaves when it executes a return
  instruction. You can find details on this difference in the man page
  for uthread_create.
 */
void uthread_init(long period){
  //no mutex used in this func for simplicity, as assumed that main thread calls it before calling anyother api
  
  //intialize all the mutexes
  uthread_mutex_init(&currentThreadMutex);
  uthread_mutex_init(&idMutex);
  uthread_mutex_init(&queueMutex);
  
  uthread_cancel_retval = (int *)malloc(sizeof(int));
  *uthread_cancel_retval = UTHREAD_CANCELLED;

  //must have been called by main thread
  currentThread = &mainThread;
  mainThread.joined_tid = -1;
  mainThread.id = -2;
  mainThread.status = ST_ALIVE;
  if (getcontext(&mainThread.uc) == -1) {
    perror("Unable to getcontext for main");
    exit(-1);
  }
  mainThread.uc.uc_stack.ss_sp = (char*) malloc(SIGSTKSZ);
  mainThread.uc.uc_stack.ss_size = SIGSTKSZ;

  // Setting up the signal set and mask
  sigemptyset(&ut_sigset);
  sigaddset(&ut_sigset, SIGVTALRM);
  unblock_signals();

  // Setting up the handler
  struct sigaction act;
  memset (&act, '\0', sizeof(act));
  act.sa_handler = &scheduler;
  if (sigaction(SIGVTALRM, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  steque_init(&ut_queue);  
  
  if (period > 0) {
    struct itimerval* timer = (struct itimerval*) malloc(sizeof(struct itimerval));
    timer->it_value.tv_sec = timer->it_interval.tv_sec = 0;
    timer->it_value.tv_usec = timer->it_interval.tv_usec = period;
    setitimer(ITIMER_VIRTUAL, timer, NULL);
  }
}


/*
  The uthread_create() function mirrors the uthread_create() function,
  only default attributes are always assumed.
 */
int uthread_create(uthread_t *thread,
            void *(*start_routine)(void *),
            void *arg){
  
  uthread_mutex_lock(&idMutex);
  thread->id = idCounter++;
  uthread_mutex_unlock(&idMutex);
  if (thread->id > UT_MAX_N_THREADS) {
    perror("Max thread count reached!");
    return -1;
  }
  
  thread->status = ST_ALIVE;
  thread->joined_tid = -1;

  if (getcontext(&thread->uc) == -1) {
    perror("Unable to getcontext"); 
    return -1;
  }
  thread->uc.uc_stack.ss_sp = (char*) malloc(SIGSTKSZ);
  thread->uc.uc_stack.ss_size = SIGSTKSZ;
  uthread_mutex_lock(&currentThreadMutex);
  thread->uc.uc_link = &currentThread->uc;
  uthread_mutex_unlock(&currentThreadMutex);
  makecontext(&thread->uc, (void (*) ())run_thread, 2, start_routine, arg);

  threads[thread->id] = thread;
  
  uthread_mutex_lock(&queueMutex);
  steque_enqueue(&ut_queue, thread);
  uthread_mutex_unlock(&queueMutex);
  return 0;
}

/*
  The uthread_join() function is analogous to uthread_join.
  All uthreads are joinable.
 */
int uthread_join(uthread_t thread, void **status) {
  uthread_mutex_lock(&currentThreadMutex);
  if (uthread_equal(thread, *currentThread)) {//can't join self
    uthread_mutex_unlock(&currentThreadMutex);
    return -1;
  }

  uthread_t* joinedThread = threads[thread.id];
  if (currentThread->joined_tid == joinedThread->id) {//deadlock check
    uthread_mutex_unlock(&currentThreadMutex);
    return -1;
  }

  joinedThread->joined_tid = currentThread->id;//applymutex
  uthread_mutex_unlock(&currentThreadMutex);

  while ((joinedThread->status != ST_FINISHED) && (joinedThread->status != ST_CANCELLED)) {
    uthread_yield();
  }

  if (status != NULL) {
    *status = joinedThread->retval;
  }

  return 0;
}

/*
  The uthread_exit() function is analogous to uthread_exit.
 */
void uthread_exit(void* retval) {
    uthread_mutex_lock(&currentThreadMutex);
    currentThread->retval = retval;
    if(uthread_equal(*currentThread, mainThread)){
        uthread_mutex_unlock(&currentThreadMutex);
        uthread_mutex_lock(&queueMutex);
        while(!steque_isempty(&ut_queue)){
            uthread_mutex_unlock(&queueMutex);
            uthread_yield();
            uthread_mutex_lock(&queueMutex);
        }
        exit(*(int*)mainThread.retval);
    }
    currentThread->status = ST_FINISHED;// main thread never to be set to finished
    uthread_mutex_unlock(&currentThreadMutex);
    uthread_yield();
}

/*
  The uthread_yield() function is analogous to uthread_yield, causing
  the calling thread to relinquish the cpu and place itself at the
  back of the schedule queue.
 */
void uthread_yield(void){
  struct itimerval timer;
  if(getitimer(ITIMER_VIRTUAL, &timer) == 0){
    timer.it_value.tv_usec = timer.it_interval.tv_usec;
  }
  raise(SIGVTALRM);
}


/*
  The uthread_yield() function is analogous to uthread_equal,
  returning zero if the threads are the same and non-zero otherwise.
 */
int uthread_equal(uthread_t t1, uthread_t t2){
  return t1.id == t2.id ? 1 : 0;
}

/*
  The uthread_cancel() function is analogous to uthread_cancel,
  allowing one thread to terminate another asynchronously.
 */
int uthread_cancel(uthread_t thread){
  int yield = 0;
  if(threads[thread.id]->status == ST_ALIVE){ //applymutex
    if(uthread_equal(thread, uthread_self())){
        yield = 1;
    }       

    threads[thread.id]->status = ST_CANCELLED;
    threads[thread.id]->retval = uthread_cancel_retval;
    if(yield != 0){
        uthread_yield();
    }
    return 0;
  }
  else{
    fprintf(stderr, "Thread: %d is no more alive\n", thread.id);
    return -1;
  }
}

/*
  Returns calling thread.
 */
uthread_t uthread_self(void) {
  uthread_mutex_lock(&currentThreadMutex);
  uthread_t currT =  *currentThread;//apply mutex
  uthread_mutex_unlock(&currentThreadMutex);
  return currT;
}

