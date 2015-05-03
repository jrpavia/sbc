#define _GNU_SOURCE
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/file.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


//Cabecera para extraer los datos de forma sencilla del bloque leido
typedef struct __header {
  char cabecera[16]; //RESPONSE*FROM*IO
  char reserved[3]; //Normal a 0
  char espera; //Si es \x60 hay que esperar la respuesta
  char reserved2;
  char length; //Longitud de los datos
  char reserved3;
  char nsecuencia;
  char reserved4[2];
  char msbid;
  char lsbid;
  char reserved5[4];
  char data[480]; // 512 - 16*2
} HEADER;

//Tipo de operacion
enum operacion_t {
  INIT = 0xf0
  ,ATR = 0x01
  ,FW  = 0xfd
  ,APDU= 0x08
};

/*
 * Abre el fichero y devuelve el identificador.
 * Devuelve <0 si ha ocurrido cualquier error, y >0 en caso contrario.
 */
int sd_open(char *fichero);

/*
 * Cierra el fichero
 */
int sd_close(int tj);

/*
 * Leemos de la tarjeta.
 * Devuelve <0 si error, y 0 en caso contrario
 * Resultado es la variable global readblk
 */
//int sd_read(int tj);

/*
 * Escribimos en la tarjeta.
 * Devuelve <0 si error, y 0 en caso contrario
 * El bloque de escritura en la variable global writeblk
 */
//int sd_write(int tj, enum operacion_t oper, char *params, int size);

/*
 * Enviamos apdu a la tarjeta
 * Como resultado de la función devuelve:
 *     -1 si error
 *     -2 si salida es mayor que szsalida
 *     >0 la longitud de los datos devueltos
 *
 */
int send_apdu(int tj, char *apdu, int szapdu, char *salida, int szsalida);

/*
 * Inicializa la tarjeta y devuelve el ATR
 * Como resultado de la función devuelve:
      -1 si error
      -2 si salida es mayor que szsalida
      >0 la longitud de los datos devueltos
 */
int sd_init(int tj, char *atr, int szatr);

