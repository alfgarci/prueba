#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "comun.h"

void R10();
void R12();
void R14();
void visualiza(int cola, int parada, int inout, int pintaborra, int destino);

int llega12 = 0, llega10 = 0, llega14=0, pidbus;;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int colagrafica, llega, sale, colaparada, tuberia;
  struct tipo_parada pasajero;
  char nombrefifo[10];
  int fifosalir, testigo = 1;
  struct ParametrosCliente params;

  srand(getpid());
  signal(10, R10); // me preparo para la senyal 10
  signal(12, R12); // me preparo para la senyal 12
  signal(14, R14); // me preparo para la senyal 14

  // Creamos y abrimos la cola de mensajes
  colagrafica = crea_cola(ftok("./fichcola.txt", 18));
  colaparada = crea_cola(ftok("./fichcola.txt", 20));

  // movemos la pipe a otra posicion y recuperamos la salida de errores
  tuberia = dup(2);
  close(2);
  open("/dev/tty", O_WRONLY);
  // Leemos los parametros desde la pipe
  read(tuberia, &params, sizeof(params));
  // Leemos elpidbus desde la pipe
  read(tuberia, &pidbus, sizeof(pidbus));

  // Generamos aleatoriamente la parada de llegada y la de salida
  srand(getpid());
  llega = rand() % (params.numparadas) + 1;
  sale = rand() % (params.numparadas) + 1;
  // Si la parada de llegada es la misma que la de salida, generamos otra
  while (llega == sale)
    sale = rand() % (params.numparadas) + 1;

  // Abrimos la fifo de la parada de salida
  snprintf(nombrefifo, 10, "fifo%d", sale);
  fifosalir = open(nombrefifo, O_RDONLY);
  if (fifosalir == -1)
    perror("Error al abrir la fifo de parada");

  // Nos pintamos en la parada
  visualiza(colagrafica, llega, IN, PINTAR, sale);
  // Enviamos la peticion a la cola de paradas
  pasajero.tipo = llega;
  pasajero.pid = getpid();
  pasajero.destino = sale;
  if (msgsnd(colaparada, (struct tipo_parada *)&pasajero,
             sizeof(struct tipo_parada) - sizeof(long), 0) == -1)
    perror("Error al escribir en la cola de paradas");

  //Lanzo la alarma
  alarm(rand() % (params.aburrimientomax - params.aburrimientomin + 1) +
        params.aburrimientomin);
  // Espero a que el bus me recoja, cuando me recoja el bus me manda la señal 12
  if (!llega12)
    pause();
  //Puede llegar la 12 o la 14
  if(llega12 ){
    alarm(0); // Desactivo la alarma
    llega12 = 0;

    visualiza(colagrafica, llega, IN, BORRAR, sale);
    visualiza(colagrafica, 0, 0, PINTAR, sale);

    // Espero a que el bus me deje en la parada de salida, cuando tenga el testigo
    // en la fifo de salida
    if (read(fifosalir, &testigo, sizeof(testigo)) <= 0)
      perror("Error al leer la fifo de salida");

    visualiza(colagrafica, 0, 0, BORRAR, sale);
    visualiza(colagrafica, sale, OUT, PINTAR, sale);

  }else{
    //Es la 14 la que ha llegado
    visualiza(colagrafica, llega, IN, BORRAR, sale);//Borro de la cola 
    visualiza(colagrafica, 7, OUT, PINTAR, sale);// Pinto cera OUT = 0

    //Esperamos a la 12 porque si o si va a llegar, es cuando vamos a contentar si se monta o no 
    if (!llega12)
      pause();
  }
  
  // cerramos la fifo de salida
  close(fifosalir);

  return 0;
}

/************************************************************************/
/***********   FUNCION: visualiza     ***********************************/
/************************************************************************/
// Pinta o borra en el servidor gráfico

void visualiza(int cola, int parada, int inout, int pintaborra, int destino) {
  struct tipo_elemento peticion;

  peticion.tipo = 2; // Los clientes son tipo 2, el autobus tipo 1
  peticion.pid = getpid();
  peticion.parada = parada;
  peticion.inout = inout;
  peticion.pintaborra = pintaborra;
  peticion.destino = destino;

  if (msgsnd(cola, (struct tipo_elemento *)&peticion,
             sizeof(struct tipo_elemento) - sizeof(long), 0) == -1)
    perror("Error al enviar a la cola de mensajes del servidor");

  if (pintaborra == PINTAR) {
    if (!llega10)
      pause(); // espero conformidad de que me han pintado, sino me mataran
    llega10 = 0;
  }
}

/************************************************************************/
/***********    FUNCION: R10     ****************************************/
/************************************************************************/

void R10() { llega10 = 1; }

/************************************************************************/
/***********    FUNCION: R12     ****************************************/
/************************************************************************/

void R12() { 
  if(llega14){
    //NO ME MONTO
     if(kill(pidbus, 5)==1)
      perror("CLIENTE: error señal 5, no se monta ");
  }else{
    //ME MONTO
     if(kill(pidbus, 6)==1)
      perror("CLIENTE: error señal 6, se monta ");

  }
  llega12 = 1;
}

/************************************************************************/
/***********    FUNCION: R14     ****************************************/
/************************************************************************/

void R14() { llega14 = 1; }
