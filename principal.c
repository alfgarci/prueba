#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "comun.h"

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax);
int creaproceso(const char *, int);
int creaservigraf(int);
void R10();
void R12();

int llega10 = 0;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int pservidorgraf, i, pidbus;
  int tubocliente[2], tubobus[2];
  char nombrefifo[10];
  int fifos[7];

  struct ParametrosBus parambus;
  struct ParametrosCliente paramclientes;
  int maxclientes, creamin, creamax;

  signal(10, R10);
  signal(12, R12);
  srand(getpid());

  // Leemos los parámetros desde el teclado
  leeparametros(&parambus, &paramclientes, &maxclientes, &creamin, &creamax);

  // Lanzamos el servidor gráfico
  // El servidor gráfico se lanza con la última parada como argumento
  pservidorgraf = creaservigraf(
      parambus.numparadas); // El argumento es la ultima parada. Maximo 6
  if (!llega10)
    pause(); // Espero a que el servidor gráfico de el OK

  // creamos las fifos. Las abrimos para que luego no haya que esperar en
  // aperturas síncronas
  for (i = 1; i <= parambus.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    unlink(nombrefifo); // por si se quedo de una ejecución previa
    if (mkfifo(nombrefifo, 0600) == -1)
      perror("Error al crear la fifo");
    fifos[i] = open(nombrefifo, O_RDWR);
    if (fifos[i] == -1)
      perror("Errro al abrir la fifo");
  }

  // Creamos las pipes para los parametos de bus y cliente
  pipe(tubobus);
  pipe(tubocliente);

  // Creamos el proceso bus, pasandole la lectura de la pipe
  pidbus = creaproceso("bus", tubobus[0]);
  // Escribimos los parametros al bus, por la pipe
  if (write(tubobus[1], &parambus, sizeof(parambus)) == -1)
    perror("error al escribir parametros al bus");

  for (i = 1; i <= maxclientes; i++) {
    // Creamos el proceso cliente, pasandole la lectura de la pipe
    creaproceso("cliente", tubocliente[0]);
    // Escribimos los parametros al cliente, por la pipe
    if (write(tubocliente[1], &paramclientes, sizeof(paramclientes)) == -1)
      perror("error al escribir parametros al cliente");
    // Escribimos  por la pipe el pid del bus 
    if (write(tubocliente[1], &pidbus, sizeof(pidbus)) == -1)
      perror("error al escribir el pid bus al cliente");
    sleep(rand() % (creamax - creamin + 1) + creamin);
  }

  // Esperamos que todos los clientes finalicen
  for (i = 1; i <= maxclientes; i++)
    wait(NULL);
  sleep(2);

  // Avisamos al bus para que termine
  if (kill(pidbus, 12) == -1) {
    perror("Error al enviar 12 al bus");
    exit(-1);
  }

  // Avisamos al servidor grafico para que termine
  if (kill(pservidorgraf, 12) == -1) {
    perror("Error al enviar 12 al servidor grafico");
    exit(-1);
  }
  
  // Cerramos y borramos las fifos
  for (i = 1; i <= parambus.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    close(fifos[i]);
    unlink(nombrefifo);
  }

  //Esperar la finalizacion del servidor grafico y del bus	
  wait(NULL);
  wait(NULL);

  // Limpiamos la terminal de restos graficos
  system("reset");
  return 0;
}

/************************************************************************/
/***********    FUNCION: leeparametros     ******************************/
/************************************************************************/

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax) {
  int ok = 0;

  *maxclientes = 10; // Numero de clientes que se crearan
  *creamin = 1;      // Intervalo de tiempo para crear nuevos clientes MIN
  *creamax = 5;      // Intervalo de tiempo para crear nuevos clientes MAX
  parambus->numparadas = paramclientes->numparadas = 6; // Cantidad de paradas
  parambus->capacidadbus = 5;                           // Capacidad del bus
  parambus->tiempotrayecto = 3;        // Tiempo del trayecto entre paradas
  paramclientes->aburrimientomax = 20; // Intervalo de tiempo en aburrirse MAX
  paramclientes->aburrimientomin = 10; // Intervalo de tiempo en aburrirse MIN

  while (ok == 0) {
    system("clear");
    printf("Valores de los parámetros...\n\n");
    printf("Numero de pasajeros que se crearan: %d\n", *maxclientes);
    printf("Intervalo de tiempo para crear nuevos pasajeros: [%d-%d] \n",
           *creamin, *creamax);
    printf("Número de paradas: %d\n", parambus->numparadas);
    printf("Capacidad del Bus: %d\n", parambus->capacidadbus);
    printf("Tiempo en el trayecto entre paradas: %d\n",
           parambus->tiempotrayecto);
    printf("Intervalo de tiempo de aburrimiento: [%d-%d]\n",
           paramclientes->aburrimientomin, paramclientes->aburrimientomax);
    printf("Pulse 0 si desea introducir nuevos valores, cualquier otro valor "
           "si desea continuar.\n");
    scanf("%d", &ok);

    if (ok == 0) {
      do {
        printf("Numero de pasajeros que se crearan [maximo 50]:\n");
        scanf("%d", maxclientes);
      } while (*maxclientes <= 0 || *maxclientes > 50);

      do {
        printf("Intervalo de tiempo para crear nuevos pasajeros MIN [entre 1 y "
               "8]: \n");
        scanf("%d", creamin);
      } while (*creamin < 1 || *creamin > 8);

      do {
        printf("Intervalo de tiempo para crear nuevos pasajeros MAX [entre 2 y "
               "20]: \n");
        scanf("%d", creamax);
      } while (*creamax < 2 || *creamax > 20 || *creamax <= *creamin);

      do {
        printf("Número de paradas: \n");
        scanf("%d", &parambus->numparadas);
      } while (parambus->numparadas < 2 || parambus->numparadas > 6);
      paramclientes->numparadas = parambus->numparadas;

      do {
        printf("Capacidad del bus [maximo 10]: \n");
        scanf("%d", &parambus->capacidadbus);
      } while (parambus->capacidadbus <= 0 || parambus->capacidadbus > 10);

      do {
        printf("Tiempo en el trayecto entre paradas [maximo 10]:\n");
        scanf("%d", &parambus->tiempotrayecto);
      } while (parambus->tiempotrayecto < 1 || parambus->tiempotrayecto > 10);

      do {
        printf("Intervalo de tiempo en esperar para aburrirse MIN [entre 10 y "
               "20]:\n");
        scanf("%d", &paramclientes->aburrimientomin);
      } while (paramclientes->aburrimientomin < 1 ||
               paramclientes->aburrimientomin > 10);

      do {
        printf("Intervalo de tiempo en esperar para aburrirse MAX [entre 15 y "
               "40]:\n");
        scanf("%d", &paramclientes->aburrimientomax);
      } while (paramclientes->aburrimientomax < 5 ||
               paramclientes->aburrimientomax > 20 ||
               paramclientes->aburrimientomin > paramclientes->aburrimientomax);
    }
  }
}

/************************************************************************/
/***********     FUNCION: creaproceso      ******************************/
/************************************************************************/

int creaproceso(const char nombre[], int tubo) {

  int vpid;

  vpid = fork();
  if (vpid == 0) {
    close(2);
    dup(tubo);
    execl(nombre, nombre, NULL);
    perror("error de execl");
    exit(-1);
  } else if (vpid == -1) {
    perror("error de fork");
    exit(-1);
  }
  return vpid;
}

/************************************************************************/
/***********    FUNCION: creaservigraf     ******************************/
/************************************************************************/

// Lanza el servidor gráfico
int creaservigraf(int ultimaparada) {

  int vpid;
  char cadparada[10];

  snprintf(cadparada,10, "%d", ultimaparada);
  
  vpid = fork();
  if (vpid == 0) {
    execl("servidor_ncurses", "servidor_ncurses", cadparada, NULL);
    perror("error de execl");
    exit(-1);
  } else if (vpid == -1) {
    perror("error de fork");
    exit(-1);
  }
  return vpid;
}

/************************************************************************/
/***********    FUNCION: R10     ****************************************/
/************************************************************************/

void R10() { llega10 = 1; }

/************************************************************************/
/***********    FUNCION: R12     ****************************************/
/************************************************************************/

void R12() {
  printf("No es posible arrancar el servidor gráfico\n");
  exit(-1);
}
