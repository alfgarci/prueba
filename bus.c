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

// Function prototypes
void pinta(int cola, int parada);
void R10(); // SIGUSR1 from ncurses
void R5();  // SIGUSR1 from cliente (no sube)
void R6();  // SIGUSR2 from cliente (si sube)
void Rfin(); // SIGUSR2 from principal (terminar) - matches original signal 12

// Global variables
int llega10 = 0, llega5 = 0, llega6 = 0;
int colaparada;
// int bajarse; // bajarse seems unused, consider removing if confirmed

// Globals for revisor interaction
pid_t g_pid_revisor = -1;
int g_pipe_escritura_revisor = -1;


/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main(int argc, char *argv[]) { // Added argc, argv for new parameters

  int colagrafica, parada, tuberia;
  struct tipo_parada pasajero;
  int libres;
  int montados[7], gente, i, testigo = 1;
  struct ParametrosBus params;

  char nombrefifo[10];
  int fifos[7];

  srand(getpid());
  signal(SIGUSR1, R10); // Assuming 10 is SIGUSR1 (ncurses ready)
  signal(SIGUSR2, Rfin); // Assuming 12 is SIGUSR2 (principal tells bus to end)
  signal(SIGRTMIN+1, R5);   // Using a real-time signal for client "no sube" (was 5)
  signal(SIGRTMIN+2, R6);   // Using a real-time signal for client "si sube" (was 6)
  
  // Parameter parsing for revisor PID and pipe FD
  // This is a simplification as per subtask instructions.
  // Assumes principal.c will pass these as the last two CLI arguments.
  if (argc < 3) { // Basic check, bus itself might have other args from creaproceso if execl was used differently
      fprintf(stderr, "Bus: Error - Se esperaban argumentos para PID del revisor y pipe FD.\n");
      // If bus name is argv[0], then argv[1] is pid_revisor, argv[2] is pipe_fd
      // However, 'creaproceso' in principal.c uses execl(nombre, nombre, NULL),
      // so argv[0] is "bus", and no other args are passed via argv by creaproceso.
      // This needs principal.c to be changed to use e.g. execlp or pass args.
      // For now, let's assume principal.c is modified to pass them as argv[1] and argv[2]
      // for a standalone test or future modification.
      // A more robust way would be to pass them through the initial pipe "tuberia"
      // like 'params', but sticking to CLI args for this step.
      // fprintf(stderr, "Uso: %s <pid_revisor> <pipe_fd_escritura_revisor> [otros_args_si_existen]\n", argv[0]);
      // exit(EXIT_FAILURE); // Cannot exit yet, need to see how 'creaproceso' is actually called
  } else {
      // This part is tricky because 'creaproceso' in the provided principal.c
      // does `execl(nombre, nombre, NULL);` which means bus.c's main will only have
      // argv[0] = "bus".
      // To make this work as per "pass as command line", principal.c's creaproceso for bus
      // would need to change to something like:
      // execl("./bus", "bus", pid_revisor_str, pipe_fd_str, (char*)NULL);
      // For this subtask, we'll write bus.c AS IF it receives them.
      // The actual passing from principal.c is a SEPARATE subtask.
      // Let's assume they are passed starting from argv[1] if principal.c is adapted.
      // This is a placeholder for where the argument parsing would go.
      // For now, these will remain -1 unless principal.c is updated to pass them.
      // This will be handled in a later step when modifying principal.c to launch bus with these args.
      // g_pid_revisor = atoi(argv[1]);
      // g_pipe_escritura_revisor = atoi(argv[2]);
      // printf("Bus: PID Revisor: %d, Pipe Revisor: %d (Placeholder from CLI)\n", g_pid_revisor, g_pipe_escritura_revisor);
  }


  // Creamos y abrimos la cola de mensajes
  colagrafica = crea_cola(ftok("./fichcola.txt", 18));
  colaparada = crea_cola(ftok("./fichcola.txt", 20));

  // movemos la pipe a otra posicion y recuperamos la salida de errores
  tuberia = dup(2);
  close(2);
  open("/dev/tty", O_WRONLY);
  // leemos los parametros desde la pipe (original mechanism)
  read(tuberia, &params, sizeof(params));
  // TODO: principal.c needs to be modified to send pid_revisor and pipe_escritura_revisor
  // via this pipe as well, by adding them to struct ParametrosBus or sending separately.
  // For THIS subtask, we are *assuming* they come via argv as per simplification,
  // but the above argv parsing block is currently non-functional with current principal.c.
  // The interaction logic below will use g_pid_revisor and g_pipe_escritura_revisor.
  // These will be -1 until principal.c is actually modified to provide them.
  // This is a known limitation of the current step.

  // TEMPORARY: Hardcode for testing if not passed by argv (remove for final integration)
  // if (argc > 2) { // If passed via command line as per future principal.c modification
  //    g_pid_revisor = atoi(argv[1]);
  //    g_pipe_escritura_revisor = atoi(argv[2]);
  //    printf("Bus %d: Revisor PID %d and Pipe FD %d received from argv.\n", getpid(), g_pid_revisor, g_pipe_escritura_revisor);
  // } else {
  //    printf("Bus %d: Revisor PID and Pipe FD not received from argv. Using defaults if any, or they will be -1.\n", getpid());
  // }


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
    if (llega6) // Cliente se monta
    {
      libres--;
      montados[pasajero.destino]++;
      llega6 = 0;
    }
    else // Cliente no se monta (llega5 o timeout)
    {
      llega5 = 0; 
    }
    usleep(RETARDO); 
  }
} // Fin de montaje de pasajeros

      // ---- INICIO LÓGICA DE INTERACCIÓN CON REVISOR ----
      if (g_pid_revisor != -1 && g_pipe_escritura_revisor != -1) {
          int pasajeros_a_bordo = params.capacidadbus - libres;
          printf("Bus %d en parada %d: Notificando al revisor (PID %d). Pasajeros a bordo: %d.\n",
                 getpid(), parada, g_pid_revisor, pasajeros_a_bordo);

          // Enviar señal al revisor de que el bus está listo en esta parada
          if (kill(g_pid_revisor, SIGUSR2) == -1) {
              perror("Bus: Error al enviar SIGUSR2 al revisor");
              // No necesariamente fatal, el revisor podría estar esperando o no.
          }

          // Enviar número de pasajeros al revisor a través del pipe
          if (write(g_pipe_escritura_revisor, &pasajeros_a_bordo, sizeof(int)) == -1) {
              perror("Bus: Error al escribir en el pipe para el revisor");
              // Podría ser que el revisor ya no exista o el pipe esté cerrado.
              // Considerar cerrar el pipe desde el bus si hay error EPIPE.
              if (errno == EPIPE) {
                  printf("Bus: Error EPIPE en pipe revisor. Cerrando extremo de escritura.\n");
                  close(g_pipe_escritura_revisor);
                  g_pipe_escritura_revisor = -1; // Marcar como cerrado
              }
          }
      }
      // ---- FIN LÓGICA DE INTERACCIÓN CON REVISOR ----

      // Pintamos al bus entre la parada y la siguiente	  
      if (parada == params.numparadas)
        pinta(colagrafica, parada * 10 + 1); // Hacia la parada 1 (circular)
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

void pinta(int cola, int parada) {
  struct tipo_elemento peticion;

  peticion.tipo = 1; // Los clientes son tipo 2, el autobus tipo 1
  peticion.pid = getpid();
  peticion.parada = parada;
  peticion.inout = 0;      // no se usa
  peticion.pintaborra = 0; // no se usa
  peticion.destino = 0;    // no se usa

  if (msgsnd(cola, (struct tipo_elemento *)&peticion,
             sizeof(struct tipo_elemento) - sizeof(long), 0) == -1)
    perror("error al enviar mensaje a cola");
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
  printf("Bus %d: Recibida señal de finalización. Limpiando...\n", getpid());
  if (g_pipe_escritura_revisor != -1) {
    printf("Bus %d: Cerrando pipe de escritura al revisor FD %d.\n", getpid(), g_pipe_escritura_revisor);
    close(g_pipe_escritura_revisor);
    g_pipe_escritura_revisor = -1;
  }
  if (colaparada != -1) { // Asegurarse que colaparada fue inicializada
      msgctl(colaparada, IPC_RMID, 0); 
      printf("Bus %d: Cola de mensajes de paradas eliminada.\n", getpid());
  }
  // Aquí se podrían cerrar también las FIFOs si es necesario, aunque principal.c las borra.
  exit(0);
}

/************************************************************************/
/***********    FUNCION: R5 NO SE MONTA     *****************************/
/************************************************************************/

void R5() { 
    // printf("Bus %d: R5 (cliente no sube) activada.\n", getpid());
    llega5 = 1; 
}

/************************************************************************/
/***********    FUNCION: R6 SE MONTA     ********************************/
/************************************************************************/

void R6() { 
    // printf("Bus %d: R6 (cliente sí sube) activada.\n", getpid());
    llega6 = 1; 
}
