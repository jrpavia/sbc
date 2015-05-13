#ifndef COMMON_H_
#define COMMON_H_

#define QUEUE_NAME     "/sbcd"
#define QUEUE_NAMEGSM  "/sbcdgsm"
#define MAX_SIZE       1024

#define MSG_STOP       99
#define MSG_SIGN        0

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \

enum modem_acciones {
        MODEM_MODULO_AT, //PING si activo y velocidad auto
        MODEM_MODULO_ECHO,   //No echo
        MODEM_MODULO_CREG,   //Para saber si estamos registrados en la red o no.
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


#endif /* #ifndef COMMON_H_ */
