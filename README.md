# User-level-threads-POSIX-compliant-
This is a user level thread package with APIs similar to Pthreads
The scheduler is a preemptive round robin scheduler. Preemption is achieved by using an alarm signal as a timer.
Any program using this package must call the function uthread_init() first.

To build the library:
1) cd src
2) make
