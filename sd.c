#define _GNU_SOURCE
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/file.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "sd.h"

//#define DEBUG

//Bloques de memoria alineados a 512 para qeu el DMA funcione (O_DIRECT)
static char writeblk[512] __attribute__ ((__aligned__ (512)));
static char readblk[512] __attribute__ ((__aligned__ (512)));

/*
 * Abre el fichero y devuelve el identificador.
 * Devuelve <0 si ha ocurrido cualquier error, y >0 en caso contrario.
 */
int sd_open(char *fichero) {
  int tj;

  //Abrimos el fichero y en caso de error, devolvemos este.
  if( (tj = open(fichero, O_RDWR|O_SYNC|O_DIRECT |0x100000)) <0)
    return tj;

  flock(tj, LOCK_EX|LOCK_NB);

  return tj;
}

/*
 * Cierra el fichero
 */
int sd_close(int tj) {
  close (tj);
  return 0;
}

/*
 * Leemos de la tarjeta.
 * Devuelve <0 si error, y 0 en caso contrario
 * Resultado es la variable global readblk
 */
int sd_read(int tj) {
#ifdef DEBUG
  int i;
#endif
  int iters;
  struct timespec ts;  //Tiempo espera

  //Leemos con una breve pausa
  //Si la tarjeta nos solicita mas tiempo, se lo concedemos
  do {
    ts.tv_sec=0;
    ts.tv_nsec = 2000000;
    nanosleep(&ts, NULL);
    lseek(tj, 0, SEEK_SET);
    if (read(tj, readblk, sizeof(readblk)) != sizeof(readblk) )
      return -1;
    iters++;
  }
  while ( ((HEADER *)readblk)->espera == 0x60 || iters==20);

  if(iters==20) return -1;

#ifdef DEBUG
  fprintf(stdout, "Leemos...\n\r");
  for(i=0; i<sizeof(readblk); i++) {
    if (i%8==0) printf("  ");
    if (i%16==0) printf("\n\r");
    fprintf(stdout, " %02X", (unsigned char) readblk[i]);
  }
  fprintf(stdout, "\n\r");
#endif

  return 0;
}

/*
 * Escribimos en la tarjeta.
 * Devuelve <0 si error, y 0 en caso contrario
 * El bloque de escritura en la variable global writeblk
 */
int sd_write(int tj, enum operacion_t oper, char *param, int szparam) {
  static unsigned char nsec = 1;
  static unsigned char msbid = 0x7c;
  static unsigned char lsbid = 0xc3;
  int i;

  //Inicializamos el bloque de escritura
  memset(writeblk, 0, sizeof(writeblk));
  //Paquete de envio
  sprintf(writeblk, "IO*WRITE*HEADER*");
  writeblk[19] = oper;
  writeblk[21] = szparam;
  writeblk[23] = nsec++;
  writeblk[26] = msbid;
  writeblk[27] = lsbid;

  //Si recibimos parametros, los incluimos
  if(szparam>0 && param != NULL)
    memcpy(writeblk + 32, param, szparam);

#ifdef DEBUG
  fprintf(stdout, "Escribimos...\n\r");
  for(i=0; i<sizeof(writeblk); i++) {
    if (i%8==0) printf("  ");
    if (i%16==0) printf("\n\r");
    fprintf(stdout, " %02X", (unsigned char )writeblk[i]);
  }
  fprintf(stdout, "\n\r");
#endif

  //Escribimos
  lseek(tj, 0, SEEK_SET);
  if( (i = write(tj, writeblk, sizeof(writeblk))) != sizeof(writeblk) )
    return -1;

  return 0;
}

/*
 * Envía y devuelve en la variable salida el resultado
 * Como resultado de la función devuelve:
      -1 si error
      -2 si salida es mayor que szsalida
      >0 la longitud de los datos devueltos
 */
int send_apdu(int tj, char *apdu, int szapdu, char *salida, int szsalida) {
  //Escribimos comando APDU
  if( sd_write(tj, APDU, apdu, szapdu ) <0) {
#ifdef DEBUG
    fprintf(stderr, "Error al escribir en la tarjeta (apdu0). Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }

  //Leemos el resultado APDU
  if( sd_read(tj) <0) {
#ifdef DEBUG
    fprintf(stderr, "Error al leer de la tarjeta. Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }

  //Comprobamos si es una petición de datos ( 0x00 0xC0 ), en ese caso NO tenemos cabecera,
  // y la longitud será del tamaño que se nos solicita el bloque
  fprintf(stdout, "APDU: (%02X %02X)\n\r",(unsigned char)apdu[0], (unsigned char)apdu[1]);
  if((unsigned char)apdu[0] == 0x00 && (unsigned char)apdu[1] == 0xC0 ) {
    memcpy(salida, readblk, szsalida);
    return szsalida;
  }

  //Comprobamos el resultado da 0x9000
  if( ((HEADER *)readblk)->length==2 && (unsigned char)((HEADER *)readblk)->data[0] == 0x90 && (unsigned char)((HEADER *)readblk)->data[1] == 0x00) {
#ifdef DEBUG
    fprintf(stdout, "Respuesta de Applet OK! (%02X %02X)\n\r",(unsigned char)((HEADER *)readblk)->data[0], (unsigned char)((HEADER *)readblk)->data[1]);
#endif
    memcpy(salida, readblk, 2);
    return 2;
  };

  //Comprobamos si el applet espera mas datos
  if( ((HEADER *)readblk)->length==2 && (unsigned char)((HEADER *)readblk)->data[0] == 0x61 ) {
    char tmpapdu[] = {0x00,0xC0,0x00,0x00,0x00};
    tmpapdu[sizeof(tmpapdu)-1] = (unsigned char)((HEADER *)readblk)->data[1];
    if( (unsigned char)((HEADER *)readblk)->data[1] > szsalida)
      return -2;
    else
      return send_apdu(tj, tmpapdu, sizeof(tmpapdu), salida, (unsigned char)(((HEADER *)readblk)->data[1]) /*szsalida*/);
  }

#ifdef DEBUG
    fprintf(stderr, "Mensaje error del Applet (%02X %02X). Saliendo...\n\r", (unsigned char)((HEADER *)readblk)->data[0], (unsigned char)((HEADER *)readblk)->data[1]);
    exit(EXIT_FAILURE);
#endif
    return -1;
}



/*
 * Inicializa la tarjeta y devuelve el ATR
 * Como resultado de la función devuelve:
      -1 si error
      -2 si salida es mayor que szsalida
      >0 la longitud de los datos devueltos
 */
int sd_init(int tj, char *atr, int szatr) {
  //Escribimos comando INIT
  if( sd_write(tj, INIT, NULL, 0 ) <0) {
#ifdef DEBUG
    fprintf(stderr, "INIT 1:Error al escribir en la tarjeta. Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }

  //Leemos el resultado INIT
  if( sd_read(tj) <0) {
#ifdef DEBUG
    fprintf(stderr, "INIT 2:Error al leer de la tarjeta. Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }

  //Escribimos comando ATR
  if( sd_write(tj, ATR, NULL, 0 ) <0) {
#ifdef DEBUG
    fprintf(stderr, "ATR 1: Error al escribir en la tarjeta. Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }

   //Leemos el resultado ATR
  if( sd_read(tj) <0) {
#ifdef DEBUG
    fprintf(stderr, "ATR 2:Error al leer de la tarjeta. Saliendo...\n\r");
    exit(EXIT_FAILURE);
#endif
    return -1;
  }
  //Copiamos a la salida el resultado del atr
  if( ((HEADER *)readblk)->length > szatr)
    return -2;
  else {
    memcpy(atr, ((HEADER *)readblk)->data, ((HEADER *)readblk)->length);
    return ((HEADER *)readblk)->length;
  }


}
