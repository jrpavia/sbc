/*
 * The classic producer-consumer example, implemented with message queues.
 * All integers between 0 and MAX_MSG-1 should be printed exactly twice,
 * once to the right of the arrow and once to the left. 
 */

/* JDN:  gcc -lrt -lpthread <c file>*/
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <sys/fcntl.h>
#include <pthread.h>

#define MAX_MSG   100
#define OVER      (-1)
#define PMODE     0666
#define QUEUENAME "/myipc"
#define QUEUESIZE 10

/*
 * Initialize a message queue 
 */

static void
init_queue (mqd_t * mq_desc, int open_flags)
{
  struct mq_attr attr;

  /*
   * Fill in attributes for message queue 
   */
  attr.mq_maxmsg = QUEUESIZE;
  attr.mq_msgsize = sizeof (int);
  attr.mq_flags = 0;

  *mq_desc = mq_open (QUEUENAME, open_flags, PMODE, &attr);
  if (*mq_desc == (mqd_t) - 1)
    {
      perror ("mq_open failure");
      exit (EXIT_FAILURE);
    };
}

/*
 * Put an integer in the queue 
 */

static void
put (mqd_t mq_desc, int data)
{
  int status;

  status = mq_send (mq_desc, (char *) &data, sizeof (int), 1);
  if (status == -1)
    perror ("mq_send failure");
}

/*
 * Read and remove an integer from the queue 
 */

static int
get (mqd_t mq_desc)
{
  ssize_t num_bytes_received = 0;
  int data=0;

  num_bytes_received =
    mq_receive (mq_desc, (char *) &data, sizeof (int), NULL);
  if (num_bytes_received == -1)
    perror ("mq_receive failure");
  return (data);
}

/*
 * Producer thread 
 */

static void *
producer (void *data)
{
  int n;
  mqd_t mqfd;

  init_queue (&mqfd, O_CREAT | O_WRONLY);
  for (n = 0; n < MAX_MSG; n++)
    {
      printf ("%d --->\n", n);
      put (mqfd, n);
    }
  put (mqfd, OVER);
  /*
   * Done with queue, so close it 
   */
  if (mq_close (mqfd) == -1)
    perror ("mq_close failure");

  return NULL;
}

/*
 * Consumer thread 
 */

static void *
consumer (void *data)
{
  int d;
  mqd_t mqfd;

  init_queue (&mqfd, O_RDONLY);
  while (1)
    {
      d = get (mqfd);
      if (d == OVER)
	break;
      printf ("---> %d\n", d);
    }
  /*
   * Done with queue, so close it 
   */
  if (mq_close (mqfd) == -1)
    perror ("mq_close failure");

  return NULL;
}

int
main (void)
{
  pthread_t th_a, th_b;
  void *retval;

  printf ("MESSAGE QUEUE TEST STARTED\n");

  /*
   * Create the threads 
   */
  pthread_create (&th_a, NULL, producer, 0);
  pthread_create (&th_b, NULL, consumer, 0);

  /*
   * Wait until producer and consumer finish. 
   */
  pthread_join (th_a, &retval);
  pthread_join (th_b, &retval);

  /*
   * Delete the message queue 
   */
  mq_unlink (QUEUENAME);

  return 0;
}
