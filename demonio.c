#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>

#include "common.h"
#include "openssl/sha.h"
#include "sd.h"

/*
 * fichero demonio.c
 *
 * Recibe ordenes de firma desde varios dispositivos e interactua con la criptotarjeta para atenderlas.
 * Tiene varios threads:
 * - cmdclient: Recibe las ordenes desde los dispositivos de interacción con el cliente, comprueba la info
 * pasada y si todo es correcto, la reenvia al thread cmdproc. Se crean tantos treads como dispositivos
 * por los que recibiremos ordenes existan.
 * - cmdproc: Recibe las peticiones, firma cada una con la criptotarjeta, y ordena al thread gsm el envío. Solo 1 thread.
 * - cmdgsm: Envía las firmas por el modem gsm a un servicio web. Pueden haber varios threads, tantos como modems gsm.
 *
 */


// ---------------------------------------------
// -------------- FIRMA CADENA -----------------
// ---------------------------------------------
/*
 * Añade la firma en hex del bstring cadena al bstring salida si la funcion devuelve 0 y error en caso contrario.
 * -1 Error al inicializar
 * -2 Error al firmar
 * salida debe de tener un tamaño de al menos tamaño de clav/4 + 1, esto es, 2048/4 +1 = 513
 */
int firma(int tj, char *cadena, char *salida) {
  int i, len;
  static int init=0;
  char firma[513]; //Debe de ser del tamaño de la clave utilizada * 2 +1 al ser en hex
  char bufer[512];
  char cur;
  unsigned char val;

  char *hex = "0123456789ABCDEF";

  //Seleccionar aplicacion KCS-15
  char apdu0[] ={0x00,0xA4,0x04,0x00,0x0C,0xA0,0x00,0x00,0x00,0x63,0x50,0x4B,0x43,0x53,0x2D,0x31,0x35};
  //Prepara la tarjeta para firmar
  char apdu1[] ={0x00,0x22,0x41,0xB6,0x07,0x84,0x02,0x00,0x02,0x80,0x01,0x03};
  //Introducimos PIN (1234)
  char apdu2[] ={0x00,0x20,0x00,0x00,0x04,0x31,0x32,0x33,0x34};
  //Ejecuta la firma. Falta el sha256
  char apdu3[SHA256_DIGEST_LENGTH+23] ={0x00,0x2A,0x9E,0x9A,0x31,0x30,0x2F,0x30,0x0B,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x04,0x20};
  //ssl calculo sha256
  SHA256_CTX sha256;

  //Si no se ha inicializado, lo hacemos
  if(!init) {
    //Inicializamos la tarjeta
    if ( (len = sd_init(tj, bufer, sizeof(bufer))) <0)
      return -1;
    //Seleccionamos el applet
    if( send_apdu(tj, apdu0, sizeof(apdu0), bufer, sizeof(bufer)) < 0)
      return -1;
    //Enviamos el APDU de preparacion
    if( send_apdu(tj, apdu1, sizeof(apdu1), bufer, sizeof(bufer)) < 0)
       return -1;
    //Enviamos el PIN de la tarjeta
    if( send_apdu(tj, apdu2, sizeof(apdu2), bufer, sizeof(bufer)) < 0)
      return -1;
    //Ya tenemos la tarjeta inicializada
    init = 1;
  }
  // ¡¡¡¡¡¡¡¡¡¡ firma !!!!!!!!!!!!!!!!

  //Calculamos del HEX al BIN
  memset(firma, 0, sizeof(firma));
  for (i = 0; i < strlen(cadena); i++) {
    cur = cadena[i];
    if (cur >= 97) {
      val = cur - 97 + 10;
    }
    else if (cur >= 65) {
      val = cur - 65 + 10;
    }
     else {
      val = cur - 48;
    }
    // Resultado lo guardamos en el array firma. Si es par, va a los primeros 4 bytes, y el impar a los siguientes
    if (i%2 == 0)
        firma[i/2] = val << 4;
    else
        firma[i/2] |= val;
  }

  //Calculamos el sha256 de la cadena que se nos pasa
  SHA256_Init(&sha256);
  //SHA256_Update(&sha256, cadena, strlen(cadena));
  SHA256_Update(&sha256, firma, i/2);
  SHA256_Final((unsigned char *)apdu3+22, &sha256);
  apdu3[sizeof(apdu3)-1] = 0x80;

  //Enviamos el comando APDU firma a la tarjeta y comprobamos el retorno
  if( (len = send_apdu(tj, apdu3, sizeof(apdu3), bufer, sizeof(bufer))) < 0) {
    return -2;
  }
  // len debería de ser de 128 caracteres.

  //Resultado pasado a hex
  memset(salida, 0, 513);
  for(i=0; i<len; i++) {
    salida[i*2]   =  hex[ (bufer[i] & 0xF0)>>4 ];
    salida[i*2+1] =  hex[ bufer[i] & 0x0F      ];
  }
  return 0;
}

void init_queue (mqd_t * mq_desc, int open_flags) {
    struct mq_attr attr;

    // initialize the queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    // create the message queue
    *mq_desc = mq_open(QUEUE_NAME, open_flags, 0644, &attr);
    CHECK((mqd_t)-1 != *mq_desc);
}

/*
 * Thread que atiende ordenes que le llegan desde el cliente
 * Tienen la siguiente formato:
 * CMD(2) FD(2) PARAMS
 * Confiamos en que nos llega ya comprobado
 */
static void *cmdproc (void *data) {
  mqd_t mq;
  char bufer[MAX_SIZE + 1];
  char buferFirma[513];
  char buferEnvio[MAX_SIZE+1];
  int must_stop = 0;
  int fd_tj = *((int *)data);
  int fd;

  //Creamos la cola
  init_queue(&mq, O_CREAT | O_RDONLY);
  CHECK((mqd_t)-1 != mq);

  //Esperamos a la orden
  do {
    ssize_t bytes_read;
    char *endptr;

    // receive the message
    bytes_read = mq_receive(mq, bufer, MAX_SIZE, NULL);
    CHECK(bytes_read >= 0);
    bufer[bytes_read] = '\0';

    //Obtenemos la orden de la siguiente forma CMD(2) FD(2) PARAMS
    long n = strtol( bufer, &endptr, 10);
    switch(n) {
      case MSG_STOP: //Salida !!!
		must_stop=1;
		break;
      case MSG_SIGN: //Firma de mensaje
		//Leemos el fichero donde escribir
		fd = strtol( endptr+1, &endptr, 10);
		if(firma(fd_tj, endptr+1, buferFirma) == 0) {
			snprintf(buferEnvio, sizeof(buferEnvio),"OK: %s\n", buferFirma);
			write(fd, buferEnvio, strlen(buferEnvio));
		} else {
			write(fd, "ERROR: FIRMA\n", 13);
		}
		break;
    }

  } while (!must_stop);

  /* cleanup */
  CHECK((mqd_t)-1 != mq_close(mq));
  CHECK((mqd_t)-1 != mq_unlink(QUEUE_NAME));
 return 0;
}

//Thread que lee las ordenes del cliente desde el fd que se le pasa
static void *cmdclient (void *data) {
  mqd_t mq;
  char bufer[MAX_SIZE+1]; //Bufer de lectura
  size_t processed =0;  //Caracteres ya leidos
  size_t r=0;
  int fd = *((int *)data);

  //Creamos la cola
  init_queue(&mq,  O_CREAT | O_WRONLY);
  CHECK((mqd_t)-1 != mq);

  do {
     //Leemos el contenido del bufer pendiente
     r=read(fd, bufer+processed, MAX_SIZE-processed);
     CHECK(0 == errno);
     CHECK(0 <= r);
     processed += r;
     //buscamos la linea completa
     char *p = bufer;
     while (p - bufer < processed && *p != '\n' && *p != '\r')
       p++;
     //Si hemos encontrado una linea completa, la procesamos y salimos
     if(*p == '\n' || *p == '\r') {
       *p = 0;
       //Aqui procesar la linea

       //Comprobar la linea y si es correcta enviarla.
       if(strlen(bufer) > 9 && 0 == strncmp(bufer,"SIGN:", 5)) {
         char *endptr;
         long n = strtol( bufer+5, &endptr, 10);
         if (strlen(endptr+1) == n) {
           char msg[MAX_SIZE];
           //creamos el mensaje a enviar de la siguiente forma CMD(2) FD(2) PARAMS
           memset(msg, 0, sizeof(msg));
           snprintf(msg, sizeof(msg), "%02d %02d %s", MSG_SIGN, fd, endptr+1);
           //Enviar la linea procesada
           CHECK(0 <= mq_send(mq, msg, strlen(msg), 0));
           //La contestacion de la forma write(fd, "OK:CORRECTO\n",12);
         }
         else {
           write(fd, "ERROR:LONG INCORRECTA\n",22);
         }
       }
       else {
           write(fd, "ERROR:CMD INCORRECTO\n",21);
       }
       //fprintf(stdout, "PROC:%d,SZ:%d,\'%*s\'\n",processed,p-bufer,p-bufer,bufer);

       //Restamos la linea completa (=sin los \n y \r del final) que acabamos de procesar
       while (p - bufer < processed && (*p == '\n' || *p == '\r' || *p == 0))
         p++;
       processed -= p - bufer;
       memmove(bufer,p,processed);
     }
  } while (1);

}

enum modem_acciones {
        MODEM_MODULO_AT, //PING si activo y velocidad auto
        MODEM_MODULO_ECHO,   //No echo
        MODEM_MODULO_CSQ,    //AT+CSQ Para saber la señal
        MODEM_MODULO_CGATT0, //AT+CGATT? Para saber n. redes GPRS
        MODEM_MODULO_CGATT1, //AT+CGATT=1
        MODEM_MODULO_SAPBR0,  //AT+SAPBR=3,1,"Contype","GPRS" Para setup profile 1
        MODEM_MODULO_SAPBR1, //AT+SAPBR=3,1,"APN","internet.com" Para establecer APN
        MODEM_MODULO_SAPBR2, //AT+SAPBR=1,1 Para unir los dos profiles
        MODEM_MODULO_HTTPINIT, //AT+HTTPINIT Para iniciar peticion
        MODEM_MODULO_HTTPPARA0, //AT+HTTPPARA="CID",1 Para Hacer POST
        MODEM_MODULO_HTTPPARA1, //AT+HTTPPARA="URL","http://www.XXXXXXXXX.com/" Para Hacer el POST
        MODEM_MODULO_HTTPPARA2, //AT+HTTPPARA=100,10000 El 100 son los bytes que enviare incluido el \n
        MODEM_MODULO_HTTPPARA3, //Aqui se envia los parametros
        MODEM_MODULO_HTTPACTION,  //AT+HTTPACTION=1 Para lanzar la peticion POST con los datos
        MODEM_MODULO_HTTPREAD,  //AT+HTTPREAD Para leer resultado
        MODEM_MODULO_HTTPTERM,   //AT+HTTPTERM Para terminar peticion
        MODEM_MODULO_SAPBR3    //AT+SAPBR=0,1 Para desconectar. Cuando de el DEACT hemos terminado
};

//Thread que atiende peticiones de gsm y controla este
static void *cmdgsm (void *fd) {
  mqd_t mq;
  char bufer[MAX_SIZE+1]; //Bufer de lectura
  size_t processed =0;  //Caracteres ya leidos
  size_t r=0;
  int fd = *((int *)data);
  modem_acciones estado = MODEM_MODULO_AT; //Inicializar el GSM

  //Creamos la cola
  init_queue(&mq,  O_CREAT | O_WRONLY);
  CHECK((mqd_t)-1 != mq);

  do {
     //Leemos el contenido del bufer pendiente
     r = read(fd, bufer+processed, MAX_SIZE-processed);
     CHECK(0 == errno);
     CHECK(0 <= r);
     processed += r;
     //buscamos la linea completa
     char *p = bufer;
     while (p - bufer < processed && *p != '\n' && *p != '\r')
       p++;
     //Si hemos encontrado una linea completa, la procesamos y salimos
     if(*p == '\n' || *p == '\r') {
       *p = 0;
       //Aqui procesar la linea



  //Esperar a las peticiones GSM por la cola


       //Restamos la linea completa (=sin los \n y \r del final) que acabamos de procesar
       while (p - bufer < processed && (*p == '\n' || *p == '\r' || *p == 0))
         p++;
       processed -= p - bufer;
       memmove(bufer,p,processed);
     }
  } while (1);


  //Ante orden salida....
  return 0;
}

int main(int argc, char **argv)
{
  //char buffer[MAX_SIZE + 1];
  int fd_usb, fd_ser, fd_gsm, fd_tj; //Descriptores ficheros
  pthread_t th_proc, th_cliser, th_cliusb, th_gsm; //Threads
  void *retval;

  fd_usb = open("/dev/ttyGS0", O_RDWR | O_NOCTTY ); //Abrimos el gadget USB
  CHECK(0 < fd_usb);
  fd_ser = open("/dev/ttyO2" , O_RDWR | O_NOCTTY ); //Abrimos el serie RS232
  CHECK(0 < fd_ser);
  fd_gsm = open("/dev/ttyO4" , O_RDWR | O_NOCTTY ); //Abrimos el serie del modem GSM
  CHECK(0 < fd_gsm);
  fd_tj = sd_open("/media/2E53-2E4B/SMART_IO.CRD"); //Abrimos la tarjeta crypto
  CHECK(0 < fd_tj);


  /*
   * Create the threads
   */
  pthread_create (&th_proc,   NULL, &cmdproc,   (void *)&fd_tj  ); //
  pthread_create (&th_cliusb, NULL, &cmdclient, (void *)&fd_usb ); //Atendemos ordenes desde usb
  pthread_create (&th_cliser, NULL, &cmdclient, (void *)&fd_ser ); //Atendemos ordenes desde serie
  pthread_create (&th_gsm,    NULL, &cmdgsm,    (void *)&fd_gsm ); //Ordenes gsm


  /*
   * Aqui hay que esperar a recibir una señal (SIGKILL por ejemplo)
   * y en ese momento, enviar a las colas el evento para que paren y esperar
   */

  /*
   * Wait until producer and consumer finish.
   */
  pthread_join (th_gsm,    &retval);
  pthread_join (th_cliser, &retval);
  pthread_join (th_cliusb, &retval);
  pthread_join (th_proc,   &retval);

  sd_close(fd_tj);
  close(fd_usb);
  close(fd_ser);
  close(fd_gsm);

  return 0;
}
