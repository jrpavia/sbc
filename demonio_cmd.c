#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <mqueue.h>

#include "common.h"

void init_queue (mqd_t * mq_desc, int open_flags) {
    struct mq_attr attr;

    // initialize the queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    // create the message queue
    mq_desc = mq_open(QUEUE_NAME, open_flags, 0644, &attr);
    CHECK((mqd_t)-1 != mq_desc);
}

/*
 * Thread que atiende ordenes que le llegan desde el cliente
 * Tienen la siguiente formato:
 * CMD(2) FD(2) PARAMS
 * Confiamos en que nos llega ya comprobado
 */
void *cmdproc (void *data) {
  mqd_t mq;
  char buffer[MAX_SIZE + 1];
  int must_stop = 0;

  //Creamos la cola
  init_queue(&mq,  O_CREAT | O_RDONLY);

  //Esperamos a la orden
  do {
    ssize_t bytes_read;

    // receive the message
    bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
    CHECK(bytes_read >= 0);

    buffer[bytes_read] = '\0';
    if (! strncmp(buffer, MSG_STOP, strlen(MSG_STOP)))
    {
      must_stop = 1;
    }
    else
    {
      printf("Received: %s\n", buffer);
    }
  } while (!must_stop);

  /* cleanup */
  CHECK((mqd_t)-1 != mq_close(mq));
  CHECK((mqd_t)-1 != mq_unlink(QUEUE_NAME));
}

//Thread que lee las ordenes del cliente desde el fd que se le pasa
void *cmdclient (void *fd) {
}

//Thread que atiende peticiones de gsm y controla este
void *cmdgsm (void *fd) {
}

int main(int argc, char **argv)
{
  char buffer[MAX_SIZE + 1];
  int fd_usb, fd_ser, fd_gsm; //Descriptores ficheros
  pthread_t th_proc, th_cliser, th_cliusb, th_gsm; //Threads
  void *retval;

  fd_usb = open("/dev/ttyGS0", O_RDWR | O_NOCTTY ); //Abrimos el gadget USB
  CHECK(0 > fd_usb);
  fd_ser = open("/dev/ttyO2" , O_RDWR | O_NOCTTY ); //Abrimos el serie RS232
  CHECK(0 > fd_ser);
  fd_gsm = open("/dev/ttyO4" , O_RDWR | O_NOCTTY ); //Abrimos el serie del modem GSM
  CHECK(0 > fd_gsm);


  /*
   * Create the threads
   */
  pthread_create (&th_proc,   NULL, cmdproc,   0      );
  pthread_create (&th_cliusb, NULL, cmdclient, fd_usb ); //Atendemos ordenes desde usb
  pthread_create (&th_cliser, NULL, cmdclient, fd_ser ); //Atendemos ordenes desde serie
  pthread_create (&th_gsm,    NULL, cmdgsm,    fd_usb ); //Ordenes gsm


  /*
   * Wait until producer and consumer finish.
   */
  pthread_join (th_proc,   &retval);
  pthread_join (th_cliusb, &retval);
  pthread_join (th_cliser, &retval);
  pthread_join (th_gsm,    &retval);

  return 0;
}
