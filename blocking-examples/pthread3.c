#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <sys/time.h>
#include <stdlib.h>

#define NUM_THREADS		4
#define START_SERVICE 		0
#define HIGH_PRIO_SERVICE 	1
#define MID_PRIO_SERVICE 	2
#define LOW_PRIO_SERVICE 	3
#define NUM_MSGS 		3

pthread_t threads[NUM_THREADS];
pthread_attr_t rt_sched_attr[NUM_THREADS];
int rt_max_prio, rt_min_prio;
struct sched_param rt_param[NUM_THREADS];
struct sched_param nrt_param;

pthread_mutex_t msgSem;
pthread_mutexattr_t rt_safe;

int rt_protocol;

volatile int runInterference=0, CScount=0;
volatile unsigned long long workerCount[NUM_THREADS];
int intfTime=0;


void *startService(void *threadid);

unsigned int idx = 0, jdx = 1;
unsigned int seqIterations = 47;
unsigned int reqIterations = 1, Iterations = 1000;
unsigned int fib = 0, fib0 = 0, fib1 = 1;

#define FIB_TEST(seqCnt, iterCnt)      \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \


void *workerNoSem(void *threadid)
{
  struct timeval timeNow;

  do
  {
    FIB_TEST(seqIterations, Iterations);
    workerCount[*((int *)threadid)]++;
  } while(workerCount[*((int *)threadid)] < runInterference);

  gettimeofday(&timeNow, NULL);
  printf("**** %d worker NO SEM stopping at %d sec, %d usec\n", *((int *)threadid), (int)timeNow.tv_sec, (int)timeNow.tv_usec);

  pthread_exit(NULL);
}


int CScnt=0;

void *worker(void *threadid)
{
  struct timeval timeNow;

  pthread_mutex_lock(&msgSem);
  CScnt++;

  do
  {
    FIB_TEST(seqIterations, Iterations);
    workerCount[*((int *)threadid)]++;
  } while(workerCount[*((int *)threadid)] < runInterference);

  sleep(2);

  workerCount[*((int *)threadid)]=0;

  do
  {
    FIB_TEST(seqIterations, Iterations);
    workerCount[*((int *)threadid)]++;
  } while(workerCount[*((int *)threadid)] < runInterference);

  pthread_mutex_unlock(&msgSem);

  gettimeofday(&timeNow, NULL);
  printf("**** %d worker stopping at %d sec, %d usec\n", *((int *)threadid), (int)timeNow.tv_sec, (int)timeNow.tv_usec);

  pthread_exit(NULL);
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
	   printf("Pthread Policy is SCHED_FIFO\n");
	   break;
     case SCHED_OTHER:
	   printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
	   printf("Pthread Policy is SCHED_OTHER\n");
	   break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }
}

int main (int argc, char *argv[])
{
   int rc, invSafe=0, i, scope, threadidx;
   struct timeval sleepTime, dTime;

   CScount=0;

   if(argc < 2)
   {
     printf("Usage: pthread interfere-seconds\n");
     exit(-1);
   }
   else if(argc >= 2)
   {
     sscanf(argv[1], "%d", &intfTime);
     printf("interference time = %d secs\n", intfTime);
     printf("unsafe mutex will be created\n");
   }

   print_scheduler();

   pthread_attr_init(&rt_sched_attr[START_SERVICE]);
   pthread_attr_setinheritsched(&rt_sched_attr[START_SERVICE], PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr[START_SERVICE], SCHED_FIFO);

   pthread_attr_init(&rt_sched_attr[HIGH_PRIO_SERVICE]);
   pthread_attr_setinheritsched(&rt_sched_attr[HIGH_PRIO_SERVICE], PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr[HIGH_PRIO_SERVICE], SCHED_FIFO);

   pthread_attr_init(&rt_sched_attr[MID_PRIO_SERVICE]);
   pthread_attr_setinheritsched(&rt_sched_attr[MID_PRIO_SERVICE], PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr[MID_PRIO_SERVICE], SCHED_FIFO);

   pthread_attr_init(&rt_sched_attr[LOW_PRIO_SERVICE]);
   pthread_attr_setinheritsched(&rt_sched_attr[LOW_PRIO_SERVICE], PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&rt_sched_attr[LOW_PRIO_SERVICE], SCHED_FIFO);

   rt_max_prio = sched_get_priority_max(SCHED_FIFO);
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);

   rc=sched_getparam(getpid(), &nrt_param);

   if (rc) 
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }

   print_scheduler();

   printf("min prio = %d, max prio = %d\n", rt_min_prio, rt_max_prio);
   pthread_attr_getscope(&rt_sched_attr[START_SERVICE], &scope);

   if(scope == PTHREAD_SCOPE_SYSTEM)
     printf("PTHREAD SCOPE SYSTEM\n");
   else if (scope == PTHREAD_SCOPE_PROCESS)
     printf("PTHREAD SCOPE PROCESS\n");
   else
     printf("PTHREAD SCOPE UNKNOWN\n");

   pthread_mutex_init(&msgSem, NULL);

   rt_param[START_SERVICE].sched_priority = rt_max_prio;
   pthread_attr_setschedparam(&rt_sched_attr[START_SERVICE], &rt_param[START_SERVICE]);

   printf("Creating thread %d\n", START_SERVICE); threadidx=START_SERVICE;
   rc = pthread_create(&threads[START_SERVICE], &rt_sched_attr[START_SERVICE], startService, (void *)&threadidx);

   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }
   printf("Start services thread spawned\n");


   printf("will join service threads\n");

   if(pthread_join(threads[START_SERVICE], NULL) == 0)
     printf("START SERVICE done\n");
   else
     perror("START SERVICE");


   rc=sched_setscheduler(getpid(), SCHED_OTHER, &nrt_param);

   if(pthread_mutex_destroy(&msgSem) != 0)
     perror("mutex destroy");

   printf("All done\n");

   exit(0);
}


void *startService(void *threadid)
{
   struct timeval timeNow;
   int rc, threadidx;

   runInterference=intfTime;

   rt_param[LOW_PRIO_SERVICE].sched_priority = rt_max_prio-20;
   pthread_attr_setschedparam(&rt_sched_attr[LOW_PRIO_SERVICE], &rt_param[LOW_PRIO_SERVICE]);

   printf("Creating thread %d\n", LOW_PRIO_SERVICE); threadidx=LOW_PRIO_SERVICE;
   rc = pthread_create(&threads[LOW_PRIO_SERVICE], &rt_sched_attr[LOW_PRIO_SERVICE], worker, (void *)&threadidx);

   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }
   //pthread_detach(threads[LOW_PRIO_SERVICE]);
   gettimeofday(&timeNow, NULL);
   printf("Low prio %d thread spawned at %d sec, %d usec\n", LOW_PRIO_SERVICE, (int)timeNow.tv_sec, (int)timeNow.tv_usec);


   sleep(1);

   rt_param[MID_PRIO_SERVICE].sched_priority = rt_max_prio-10;
   pthread_attr_setschedparam(&rt_sched_attr[MID_PRIO_SERVICE], &rt_param[MID_PRIO_SERVICE]);

   printf("Creating thread %d\n", MID_PRIO_SERVICE); threadidx=MID_PRIO_SERVICE;
   rc = pthread_create(&threads[MID_PRIO_SERVICE], &rt_sched_attr[MID_PRIO_SERVICE], workerNoSem, (void *)&threadidx);

   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }
   //pthread_detach(threads[MID_PRIO_SERVICE]);
   gettimeofday(&timeNow, NULL);
   printf("Middle prio %d thread spawned at %d sec, %d usec\n", MID_PRIO_SERVICE, (int)timeNow.tv_sec, (int)timeNow.tv_usec);

   rt_param[HIGH_PRIO_SERVICE].sched_priority = rt_max_prio-1;
   pthread_attr_setschedparam(&rt_sched_attr[HIGH_PRIO_SERVICE], &rt_param[HIGH_PRIO_SERVICE]);


   printf("Creating thread %d, CScnt=%d\n", HIGH_PRIO_SERVICE, CScnt); threadidx=HIGH_PRIO_SERVICE;
   rc = pthread_create(&threads[HIGH_PRIO_SERVICE], &rt_sched_attr[HIGH_PRIO_SERVICE], worker, (void *)&threadidx);

   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror(NULL);
       exit(-1);
   }
   //pthread_detach(threads[HIGH_PRIO_SERVICE]);
   gettimeofday(&timeNow, NULL);
   printf("High prio %d thread spawned at %d sec, %d usec\n", HIGH_PRIO_SERVICE, (int)timeNow.tv_sec, (int)timeNow.tv_usec);





   if(pthread_join(threads[LOW_PRIO_SERVICE], NULL) == 0)
     printf("LOW PRIO done\n");
   else
     perror("LOW PRIO");

   if(pthread_join(threads[MID_PRIO_SERVICE], NULL) == 0)
     printf("MID PRIO done\n");
   else
     perror("MID PRIO");

   if(pthread_join(threads[HIGH_PRIO_SERVICE], NULL) == 0)
     printf("HIGH PRIO done\n");
   else
     perror("HIGH PRIO");


   pthread_exit(NULL);

}
