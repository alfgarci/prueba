#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "comun.h"

void pinta(int cola, int parada);
void R10();
void R5();
void R6();
void Rfin();

int llega10 = 0, llega5 = 0, llega6 = 0;
int colaparada, bajarse;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int colagrafica, parada, tuberia;
  struct tipo_parada pasajero;
  int libres;
  int montados[7], gente, i, testigo = 1;
  struct ParametrosBus params;

  char nombrefifo[10];
  int fifos[7];

  srand(getpid());
  signal(10, R10);  // me preparo para la senyal 10
  signal(12, Rfin); // me preparo para la senyal 12
  signal(5, R5); // me preparo para la senyal 5
  signal(6, R6); // me preparo para la senyal 6
  // Creamos y abrimos la cola de mensajes
  colagrafica = crea_cola(ftok("./fichcola.txt", 18));
  colaparada = crea_cola(ftok("./fichcola.txt", 20));

  // movemos la pipe a otra posicion y recuperamos la salida de errores
  tuberia = dup(2);
  close(2);
  open("/dev/tty", O_WRONLY);
  // leemos los parametros desde la pipe
  read(tuberia, &params, sizeof(params));
  libres = params.capacidadbus;

  for (i = 1; i <= params.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    fifos[i] = open(nombrefifo, O_WRONLY);
    if (fifos[i] == -1)
      perror("Error al abrir la fifo de parada");
  }
  // Inicializamos el array de montados
  // El array de montados tiene 7 posiciones, una para cada parada, e indica
  // cuantos pasajeros bajan en cada parada Inicializamos a 0
  for (i = 1; i <= params.numparadas; i++)
    montados[i] = 0;

  // El bus hace los recorridos mientras no reciba la señal 12
  while (1) {
    for (parada = 1; parada <= params.numparadas; parada++) {
      // Se visualiza en la parada
      pinta(colagrafica, parada);
      // Bajamos la gente de esa parada, segun tenemos anotado en el array
      // montados
      for (gente = 1; gente <= montados[parada];
           gente++) // Bajamos la gente de esa parada
      {
        if (write(fifos[parada], &testigo, sizeof(testigo)) == -1)
          perror("Error al escribir en la fifo");
        libres++;
        usleep(RETARDO); // para que de tiempo a pintarse
      }
      // Indicamos que no queda gente montada en esa parada
      montados[parada] = 0;
      // Montamos la gente de esa parada
      while (libres > 0 &&
       (msgrcv(colaparada, (struct tipo_parada *)&pasajero,
               sizeof(struct tipo_parada) - sizeof(long),
               parada, IPC_NOWAIT)) != -1)
{
  if (kill(pasajero.pid, 12) == -1)
  {
    perror("No se puede enviar kill a cliente");
  }
  else
  {
    if (!llega5 && !llega6)
      pause(); // espero confirmación

    // Ya el cliente ha respondido
    if (llega6)
    {
      // SE MONTA
      libres--;
      montados[pasajero.destino]++;
      llega6 = 0;
    }
    else
    {
      // NO SE MONTA
      llega5 = 0;
    }

    usleep(RETARDO); // para que dé tiempo a pintarse
  }
}
      // Pintamos al bus entre la parada y la siguiente	  
      if (parada == params.numparadas)
        pinta(colagrafica, parada * 10 + 1);
      else
        pinta(colagrafica, parada * 10 + parada + 1);
      // Tiempo de trayecto entre paradas	
      sleep(params.tiempotrayecto);
    }
  }
  return 0;
}

/************************************************************************/
/***********    FUNCION: pinta   ****************************************/
/************************************************************************/

void pinta(int cola, int parada_actual) {
  struct tipo_elemento peticion;

  peticion.tipo = 1; // Los clientes son tipo 2, el autobus tipo 1
  peticion.pid = getpid();
  peticion.parada = parada_actual;
  peticion.inout = 0;      // no se usa
  peticion.pintaborra = PINTAR; // no se usa
  peticion.destino = 0;    // no se usa

  if (msgsnd(cola, (struct tipo_elemento *)&peticion,
             sizeof(struct tipo_elemento) - sizeof(long), 0) == -1)
    perror("bus: error al enviar mensaje a cola");
  if (!llega10)
    pause(); // espero conformidad de que me han pintado, sino me mataran
  llega10 = 0;
}

/************************************************************************/
/***********    FUNCION: R10     ****************************************/
/************************************************************************/

void R10() { llega10 = 1; }

/************************************************************************/
/***********    FUNCION: Rfin    ****************************************/
/************************************************************************/

void Rfin() {
  msgctl(colaparada, IPC_RMID, 0); // Borra la cola de mensajes

  exit(0);
}

/************************************************************************/
/***********    FUNCION: R5 NO SE MONTA     *****************************/
/************************************************************************/

void R5() { llega5 = 1; }

/************************************************************************/
/***********    FUNCION: R6 SE MONTA     ********************************/
/************************************************************************/

void R6() { llega6 = 1; }
