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
// Hacer fd_pipe_conteo_revisor_bus global para que Rfin pueda acceder a él.
// Se inicializará en main después de leer los parámetros.
static int fd_pipe_conteo_revisor_bus = -1;

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int colagrafica, parada, tuberia;
  // struct tipo_parada pasajero; // Eliminada ya que pasajero_o_revisor se usa en el bucle
  int libres;
  int montados[7], gente, i, testigo = 1;
  struct ParametrosBus params;
  int pid_revisor_bus = 0;
  // fd_pipe_conteo_revisor_bus es ahora global
  int revisor_a_bordo = 0;
  int parada_subida_revisor = 0; // Esta variable se usará más adelante

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
  pid_revisor_bus = params.pid_revisor;
  // Asignar al fd_pipe_conteo_revisor_bus global
  fd_pipe_conteo_revisor_bus = params.fd_pipe_conteo_revisor;
  printf("Bus %d: Parámetros recibidos. PID Revisor: %d, FD Pipe Conteo global: %d\n", getpid(), pid_revisor_bus, fd_pipe_conteo_revisor_bus);

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

        // >>> Lógica de bajada del revisor <<<
        if (revisor_a_bordo == 1) {
            // Calcular la parada de bajada esperada para el revisor.
            // El revisor baja una parada después de la que subió, en ciclo.
            int parada_bajada_revisor_esperada = (parada_subida_revisor % params.numparadas) + 1;

            if (parada == parada_bajada_revisor_esperada) {
                printf("Bus %d: Ha llegado a la parada %d, que es la parada de bajada para el revisor (subió en %d).\n", getpid(), parada, parada_subida_revisor);
                if (kill(pid_revisor_bus, 16) == -1) { // Señal 16 para que el revisor baje
                    perror("Bus: Error al enviar señal 16 (bajar) al revisor");
                } else {
                    printf("Bus %d: Señal 16 enviada al revisor %d para bajar.\n", getpid(), pid_revisor_bus);
                }
                revisor_a_bordo = 0;
                // parada_subida_revisor = 0; // Opcional: resetear, podría ser útil si el mismo revisor pudiera subir de nuevo.
                                            // Por ahora, un revisor solo hace un ciclo.
            }
        }
        // >>> FIN: Lógica de bajada del revisor <<<

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

      // Montamos la gente de esa parada Y/O AL REVISOR
      // El msgrcv debe estar fuera de la condición de 'libres > 0' para el revisor.
      struct tipo_parada pasajero_o_revisor; // Renombrar para claridad
      while ((msgrcv(colaparada, (struct tipo_parada *)&pasajero_o_revisor,
                     sizeof(struct tipo_parada) - sizeof(long),
                     parada, IPC_NOWAIT)) != -1)
      {
          if (pasajero_o_revisor.pid == pid_revisor_bus) { // Es el revisor
              if (!revisor_a_bordo) {
                  printf("Bus %d: Revisor PID %d detectado en parada %d.\n", getpid(), pasajero_o_revisor.pid, parada);
                  if (kill(pid_revisor_bus, 12) == -1) { // Señal para que el revisor suba
                      perror("Bus: Error al enviar señal 12 (subir) al revisor");
                  } else {
                      printf("Bus %d: Señal 12 enviada al revisor %d para subir.\n", getpid(), pid_revisor_bus);
                      revisor_a_bordo = 1;
                      parada_subida_revisor = parada; // Guardar parada donde subió el revisor

                      int pasajeros_actuales = params.capacidadbus - libres;
                      printf("Bus %d: Escribiendo %d pasajeros (Capacidad: %d, Libres: %d) al revisor por pipe FD %d.\n", getpid(), pasajeros_actuales, params.capacidadbus, libres, fd_pipe_conteo_revisor_bus);
                      if (fd_pipe_conteo_revisor_bus != -1) { // Asegurarse que la pipe es válida
                          if (write(fd_pipe_conteo_revisor_bus, &pasajeros_actuales, sizeof(int)) == -1) {
                              perror("Bus: Error al escribir conteo de pasajeros al revisor");
                          }
                          // Cerrar la pipe después del primer uso, ya que es para un solo conteo.
                          close(fd_pipe_conteo_revisor_bus);
                          fd_pipe_conteo_revisor_bus = -1; // Marcar como no usable para evitar doble cierre en Rfin
                      } else {
                          printf("Bus %d: Error - fd_pipe_conteo_revisor_bus es inválido (%d).\n", getpid(), fd_pipe_conteo_revisor_bus);
                      }
                  }
                  // El revisor no ocupa sitio y no interactúa con señales 5/6 como los clientes.
              } else {
                  printf("Bus %d: Revisor PID %d detectado de nuevo en parada %d, pero ya está a bordo.\n", getpid(), pasajero_o_revisor.pid, parada);
                  // Podríamos considerar purgar este mensaje de alguna forma o simplemente ignorarlo.
                  // Por ahora se ignora.
              }
          } else { // Es un cliente normal
              if (libres > 0) {
                  printf("Bus %d: Cliente PID %d detectado en parada %d. Intentando subir (libres: %d).\n", getpid(), pasajero_o_revisor.pid, parada, libres);
                  if (kill(pasajero_o_revisor.pid, 12) == -1) {
                      //perror("Bus: No se puede enviar kill a cliente"); // Puede ser ruidoso si el cliente ya no existe
                      printf("Bus %d: No se pudo enviar señal 12 al cliente %d (puede que ya no exista).\n", getpid(), pasajero_o_revisor.pid);
                  } else {
                      llega5 = 0; // Resetear flags para este cliente
                      llega6 = 0;
                      printf("Bus %d: Esperando respuesta (señal 5 o 6) del cliente %d.\n", getpid(), pasajero_o_revisor.pid);
                      pause();

                      if (llega6) {
                          printf("Bus %d: Cliente %d confirma subida (señal 6 recibida).\n", getpid(), pasajero_o_revisor.pid);
                          libres--;
                          montados[pasajero_o_revisor.destino]++;
                          llega6 = 0;
                      } else if (llega5) {
                          printf("Bus %d: Cliente %d NO sube (señal 5 recibida).\n", getpid(), pasajero_o_revisor.pid);
                          llega5 = 0;
                      } else {
                          printf("Bus %d: Cliente %d no respondió con señal 5 o 6 después de la señal 12. Asumiendo no sube.\n", getpid(), pasajero_o_revisor.pid);
                          // Asegurarse de resetear flags si no se hizo
                          llega5 = 0;
                          llega6 = 0;
                      }
                      usleep(RETARDO);
                  }
              } else {
                  printf("Bus %d: Cliente %d no puede subir, bus lleno (libres: %d).\n", getpid(), pasajero_o_revisor.pid, libres);
                  // Opcional: enviar señal al cliente para que sepa que no subió por bus lleno.
                  // kill(pasajero_o_revisor.pid, 5); // Por ejemplo, para que sepa que no sube.
                  // Esto haría que el cliente se borre de la parada.
              }
          }
      } // Fin del while msgrcv

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
  if (fd_pipe_conteo_revisor_bus != -1) { // Si por alguna razón no se cerró antes
      printf("Bus %d: Cerrando fd_pipe_conteo_revisor_bus (%d) en Rfin.\n", getpid(), fd_pipe_conteo_revisor_bus);
      close(fd_pipe_conteo_revisor_bus);
      fd_pipe_conteo_revisor_bus = -1;
  }
  msgctl(colaparada, IPC_RMID, 0); // Borra la cola de mensajes
  printf("Bus %d: Terminando y borrando cola de mensajes.\n", getpid());
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
