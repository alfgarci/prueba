#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h> // For ftok, msgget, msgctl
#include <sys/msg.h>   // For msgget, msgctl

#include "comun.h"

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax);
int creaproceso(const char *, int);
int creaservigraf(int);
void R10();
void R12();

int llega10 = 0;
int id_cola_mensajes = -1; // Global for cleanup in case of signal exit or error

// Signal handler for cleanup
void manejador_finalizacion_principal(int sig) {
    printf("Principal %d: Recibida señal %d. Realizando limpieza de cola de mensajes...\n", getpid(), sig);
    if (id_cola_mensajes != -1) {
        if (msgctl(id_cola_mensajes, IPC_RMID, NULL) == -1) {
            // Non-critical error if already removed or bad ID
            // perror("Principal: Error al eliminar la cola de mensajes en manejador");
        } else {
            printf("Principal %d: Cola de mensajes eliminada exitosamente por señal.\n", getpid());
        }
    }
    // Propagate the signal to terminate itself after cleanup
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
}

/**********************************************************************************/
/************       MAIN ***************************************************/
/**********************************************************************************/

int main() {

  int pservidorgraf = -1, i, pidbus = -1;
  pid_t pid_revisor = -1; // PID para el proceso revisor
  int tubocliente[2], tubobus[2];
  int pipe_revisor_fd[2]; // Pipe for bus -> revisor communication
  char nombrefifo[10];
  int fifos[7];
  key_t clave_cola;

  struct ParametrosBus parambus;
  struct ParametrosCliente paramclientes;
  int maxclientes, creamin, creamax;

  signal(10, R10); // SIGUSR1
  signal(12, R12); // SIGUSR2 (originalmente para error del servidor gráfico)
  
  // Setup signal handlers for graceful cleanup on SIGINT/SIGTERM
  signal(SIGINT, manejador_finalizacion_principal);
  signal(SIGTERM, manejador_finalizacion_principal);
  
  srand(getpid());

  // Leemos los parámetros desde el teclado
  leeparametros(&parambus, &paramclientes, &maxclientes, &creamin, &creamax);
  
  // Crear clave para la cola de mensajes
  clave_cola = ftok(".", 'M'); // Usar una clave consistente
  if (clave_cola == (key_t)-1) {
    perror("Principal: Error al obtener clave para cola de mensajes con ftok");
    exit(EXIT_FAILURE);
  }

  // Crear la cola de mensajes o conectarse si ya existe
  id_cola_mensajes = msgget(clave_cola, 0600 | IPC_CREAT);
  if (id_cola_mensajes == -1) {
      perror("Principal: Error al crear/obtener la cola de mensajes con msgget");
      exit(EXIT_FAILURE);
  } else {
      printf("Principal: Cola de mensajes ID %d obtenida/creada.\n", id_cola_mensajes);
  }

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
  if (pipe(tubobus) == -1) {
      perror("Principal: Error al crear tubobus");
      exit(EXIT_FAILURE);
  }
  if (pipe(tubocliente) == -1) {
      perror("Principal: Error al crear tubocliente");
      // Limpieza de tubobus si se creó
      close(tubobus[0]); close(tubobus[1]);
      exit(EXIT_FAILURE);
  }

  // Crear el pipe para la comunicación bus -> revisor
  if (pipe(pipe_revisor_fd) == -1) {
      perror("Principal: Error al crear pipe_revisor_fd");
      // Limpieza de otras pipes
      close(tubobus[0]); close(tubobus[1]);
      close(tubocliente[0]); close(tubocliente[1]);
      exit(EXIT_FAILURE);
  }

  // ***** Lanzamiento del proceso REVISOR (antes que el bus para tener su PID) *****
  pid_revisor = fork();

  if (pid_revisor == -1) {
      perror("Principal: Error al crear proceso revisor con fork");
      // Limpieza extensiva...
      if (id_cola_mensajes != -1) msgctl(id_cola_mensajes, IPC_RMID, NULL);
      close(pipe_revisor_fd[0]); close(pipe_revisor_fd[1]);
      close(tubobus[0]); close(tubobus[1]);
      close(tubocliente[0]); close(tubocliente[1]);
      // ... (terminar otros hijos si se hubieran lanzado, como servidor gráfico)
      if(pservidorgraf != -1) kill(pservidorgraf, SIGTERM);
      exit(EXIT_FAILURE);
  } else if (pid_revisor == 0) {
      // Proceso Hijo - Revisor
      char arg_nparadas_str[10];
      char arg_idcola_str[10];
      char arg_pipe_fd_read_str[10];

      close(pipe_revisor_fd[1]); // Revisor no usa el extremo de escritura
      // Cerrar otros descriptores no necesarios por el revisor
      close(tubobus[0]); close(tubobus[1]);
      close(tubocliente[0]); close(tubocliente[1]);
      // No cerramos pipe_revisor_fd[0] aquí, lo usa execlp

      sprintf(arg_nparadas_str, "%d", parambus.numparadas);
      sprintf(arg_idcola_str, "%d", id_cola_mensajes);
      sprintf(arg_pipe_fd_read_str, "%d", pipe_revisor_fd[0]);
      
      execlp("./revisor", "revisor", arg_nparadas_str, arg_idcola_str, arg_pipe_fd_read_str, (char *)NULL);
      
      // Si execlp retorna, hubo un error
      perror("Principal: Error en execlp al lanzar revisor");
      close(pipe_revisor_fd[0]); // Asegurarse de cerrar en caso de error de execlp
      exit(EXIT_FAILURE); 
  }
  // Proceso Padre - Principal continúa aquí
  printf("Principal: Proceso REVISOR %d lanzado con pipe de lectura FD %d.\n", pid_revisor, pipe_revisor_fd[0]);


  // ***** Lanzamiento del proceso BUS (modificado para incluir PID revisor y pipe write FD) *****
  pidbus = fork();
  if (pidbus == -1) {
      perror("Principal: Error al crear proceso bus con fork");
      // Limpieza extensiva...
      kill(pid_revisor, SIGTERM); // Terminar revisor
      if (id_cola_mensajes != -1) msgctl(id_cola_mensajes, IPC_RMID, NULL);
      close(pipe_revisor_fd[0]); close(pipe_revisor_fd[1]);
      close(tubobus[0]); close(tubobus[1]);
      close(tubocliente[0]); close(tubocliente[1]);
      if(pservidorgraf != -1) kill(pservidorgraf, SIGTERM);
      exit(EXIT_FAILURE);
  } else if (pidbus == 0) {
      // Proceso Hijo - Bus
      char arg_pid_revisor_str[10];
      char arg_pipe_fd_write_str[10];

      close(pipe_revisor_fd[0]); // Bus no usa el extremo de lectura del pipe del revisor
      close(tubobus[1]);         // Bus solo lee de tubobus
      close(tubocliente[0]); close(tubocliente[1]); // Bus no usa tubocliente

      // Redirigir tubobus[0] a un FD estándar si es necesario (como hacía creaproceso con stderr/dup)
      // creaproceso hacía: close(2); dup(tubo); donde tubo era tubobus[0]
      // Esto es inusual. Asumimos que bus lee parámetros de FD 0 (stdin) si se usa dup2(tubobus[0], 0)
      // o de un FD específico si se pasa tubobus[0] como argumento.
      // Para mantener la compatibilidad con `read(tuberia, &params, sizeof(params));`
      // donde `tuberia` era `dup(2)`, y `2` fue cerrado,
      // el bus espera leer los parámetros de un FD que no es stdin.
      // El `creaproceso` original hacía `dup(tubo)` lo que significa que `tubo` (tubobus[0])
      // se duplicaba al primer FD disponible después de cerrar stderr.
      // Esto es complejo. Para esta fase, asumimos que bus.c será adaptado para leer
      // 'params' de stdin (FD 0) o que 'tubobus[0]' se pasa explícitamente
      // y bus.c lo usa en read(). La forma más simple es que bus.c lea de stdin.
      if (dup2(tubobus[0], STDIN_FILENO) == -1) { // Redirigir tubobus[0] a stdin del bus
          perror("Principal (hijo bus): Error en dup2 para tubobus[0]");
          exit(EXIT_FAILURE);
      }
      close(tubobus[0]); // Ya duplicado a stdin

      sprintf(arg_pid_revisor_str, "%d", pid_revisor);
      sprintf(arg_pipe_fd_write_str, "%d", pipe_revisor_fd[1]);

      // Lanzar bus con los nuevos argumentos para el revisor
      // La llamada a bus.c espera: ./bus <pid_revisor> <pipe_fd_write>
      // execl("./bus", "bus", arg_pid_revisor_str, arg_pipe_fd_write_str, (char *)NULL);
      // Como bus.c también tiene main(int argc, char *argv[]), y argv[0] es "bus",
      // los argumentos adicionales serán argv[1] y argv[2].
      // El código en bus.c que lee `params` de `tuberia` (que era dup(2))
      // ahora leerá de STDIN_FILENO.
      
      // execl espera path, arg0, arg1, ..., NULL
      // Si bus.c espera argv[1] y argv[2] para los nuevos params, entonces:
      execl("./bus", "bus", arg_pid_revisor_str, arg_pipe_fd_write_str, (char *)NULL);
      
      perror("Principal: Error en execl al lanzar bus");
      close(pipe_revisor_fd[1]); // Asegurarse de cerrar en caso de error de execl
      exit(EXIT_FAILURE);
  }
  // Proceso Padre - Principal continúa aquí
  printf("Principal: Proceso BUS %d lanzado, escribirá a revisor por FD %d.\n", pidbus, pipe_revisor_fd[1]);

  // El padre cierra ambos extremos del pipe del revisor, ya que los hijos tienen sus copias
  close(pipe_revisor_fd[0]);
  close(pipe_revisor_fd[1]);

  // El padre cierra el extremo de lectura de tubobus, solo escribe en él
  close(tubobus[0]);
  // Escribimos los parametros al bus, por la pipe tubobus
  if (write(tubobus[1], &parambus, sizeof(parambus)) == -1) {
    perror("Principal: error al escribir parametros al bus por tubobus");
    // Considerar terminar hijos y salir
  }
  close(tubobus[1]); // Cerrar después de escribir


  for (i = 1; i <= maxclientes; i++) {
    // Creamos el proceso cliente, pasandole la lectura de la pipe
    // `creaproceso` espera `tubocliente[0]` para que el cliente lea.
    // El padre escribe en `tubocliente[1]`.
    pid_t pid_cliente_actual = creaproceso("cliente", tubocliente[0]); // creaproceso cierra tubocliente[1] en el hijo
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
    wait(NULL); // Espera a que terminen todos los clientes
  printf("Principal: Todos los procesos cliente han terminado.\n");

  // No es necesario modificar la sección de lanzamiento de clientes aquí más allá de lo que ya está
  // a menos que creaproceso necesite ser ajustado para cerrar los nuevos pipes.
  // creaproceso no conoce pipe_revisor_fd, por lo que no los cerrará.
  // Los clientes no deberían heredar pipe_revisor_fd si se cierran en el padre antes del bucle de clientes.
  // Sin embargo, es más seguro que el padre cierre pipe_revisor_fd DESPUÉS de lanzar todos los hijos
  // que los necesiten (bus y revisor), y los hijos que NO los necesiten (clientes) los cierren ellos mismos.
  // Para esta iteración, el padre los cierra después de lanzar bus y revisor. Los clientes no los usarán.
  // El `creaproceso` es simple, por lo que no cierra FDs que no conoce.

  // Avisamos al bus para que termine (usando SIGUSR2 como en el original)
  if (pidbus != -1) {
    printf("Principal: Enviando señal 12 (SIGUSR2) al bus %d para que termine.\n", pidbus);
    if (kill(pidbus, 12) == -1) { 
      perror("Error al enviar señal 12 (SIGUSR2) al bus");
    }
  }

  // Esperar a que el revisor termine
  if (pid_revisor != -1) {
    printf("Principal: Esperando al revisor %d...\n", pid_revisor);
    waitpid(pid_revisor, NULL, 0);
    printf("Principal: Proceso revisor %d ha terminado.\n", pid_revisor);
  }

  // Avisamos al servidor grafico para que termine (usando SIGUSR2 como en el original)
  if (pservidorgraf != -1) {
    printf("Principal: Enviando señal 12 (SIGUSR2) al servidor gráfico %d para que termine.\n", pservidorgraf);
    if (kill(pservidorgraf, 12) == -1) {
      perror("Error al enviar señal 12 (SIGUSR2) al servidor grafico");
    }
  }
  
  // Cerramos y borramos las fifos
  for (i = 1; i <= parambus.numparadas; i++) {
    snprintf(nombrefifo, 10, "fifo%d", i);
    if (fifos[i] != -1) close(fifos[i]); // Solo cerrar si está abierto
    unlink(nombrefifo);
  }
  printf("Principal: FIFOs cerradas y eliminadas.\n");

  // Esperar la finalizacion del bus, y luego del servidor grafico
  if (pidbus != -1) {
    printf("Principal: Esperando finalización del bus %d...\n", pidbus);
    waitpid(pidbus, NULL, 0); // Esperar primero al bus
    printf("Principal: Proceso bus %d ha terminado.\n", pidbus);
  }
   // Esperar al revisor es después de que los clientes terminan y antes que el bus según la lógica anterior.
   // Se movió la espera del revisor para que sea después de que el bus termine,
   // asumiendo que el revisor depende del bus para completar su ciclo.
  if (pid_revisor != -1) {
    printf("Principal: Esperando al revisor %d (después del bus)...\n", pid_revisor);
    waitpid(pid_revisor, NULL, 0);
    printf("Principal: Proceso revisor %d ha terminado.\n", pid_revisor);
  }
  if (pservidorgraf != -1) {
    printf("Principal: Esperando finalización del servidor gráfico %d...\n", pservidorgraf);
    waitpid(pservidorgraf, NULL, 0);
    printf("Principal: Proceso servidor gráfico %d ha terminado.\n", pservidorgraf);
  }
  // Limpieza final de la cola de mensajes (manejador_finalizacion_principal se encarga si se sale por señal)
  // Esta es la limpieza normal al final del main
  if (id_cola_mensajes != -1) {
      if (msgctl(id_cola_mensajes, IPC_RMID, NULL) == -1) {
          perror("Principal: Error al eliminar la cola de mensajes al final del main");
      } else {
          printf("Principal: Cola de mensajes ID %d eliminada exitosamente al final del main.\n", id_cola_mensajes);
      }
      id_cola_mensajes = -1; // Marcar como eliminada
  }

  // Limpiamos la terminal de restos graficos
  printf("Principal: Limpiando terminal...\n");
  system("reset"); // Considerar si es necesario, puede limpiar mensajes de error útiles
  return 0;
}

/************************************************************************/
/***********    FUNCION: leeparametros     ******************************/
/************************************************************************/

void leeparametros(struct ParametrosBus *parambus,
                   struct ParametrosCliente *paramclientes, int *maxclientes,
                   int *creamin, int *creamax) {
  // int ok = 0; // Comentado para forzar valores por defecto

  // Valores por defecto
  *maxclientes = 10; 
  *creamin = 1;      
  *creamax = 5;      
  parambus->numparadas = 6; 
  paramclientes->numparadas = parambus->numparadas;
  parambus->capacidadbus = 5;                           
  parambus->tiempotrayecto = 3;        
  paramclientes->aburrimientomax = 20; 
  paramclientes->aburrimientomin = 10; 

  // Comentada la sección interactiva para usar siempre los valores por defecto.
  /* 
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
      } while (paramclientes->aburrimientomin < 1 || // Debería ser >=10 basado en el prompt
               paramclientes->aburrimientomin > 10); // Debería ser <=20 basado en el prompt

      do {
        printf("Intervalo de tiempo en esperar para aburrirse MAX [entre 15 y "
               "40]:\n");
        scanf("%d", &paramclientes->aburrimientomax);
      } while (paramclientes->aburrimientomax < 5 || // Debería ser >=15 basado en el prompt
               paramclientes->aburrimientomax > 20 || // Debería ser <=40 basado en el prompt
               paramclientes->aburrimientomin > paramclientes->aburrimientomax);
    }
  }
  */
  // Forzar ok=1 para usar valores por defecto sin preguntar
  // ok = 1; // No necesario si el bucle while está comentado
  
  // Imprimir valores que se usarán
  system("clear");
  printf("Usando valores de parámetros por defecto (o los últimos ingresados si se descomenta la sección interactiva y se ejecutó antes):\n\n");
  printf("Numero de pasajeros que se crearan: %d\n", *maxclientes);
  printf("Intervalo de tiempo para crear nuevos pasajeros: [%d-%d] \n", *creamin, *creamax);
  printf("Número de paradas: %d\n", parambus->numparadas);
  printf("Capacidad del Bus: %d\n", parambus->capacidadbus);
  printf("Tiempo en el trayecto entre paradas: %d\n", parambus->tiempotrayecto);
  printf("Intervalo de tiempo de aburrimiento: [%d-%d]\n\n", paramclientes->aburrimientomin, paramclientes->aburrimientomax);
  printf("Principal: Continuando con estos parámetros...\n");
  // sleep(1); // Pausa breve para que el usuario vea los parámetros
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
