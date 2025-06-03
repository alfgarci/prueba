#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/types.h> // Para sig_atomic_t
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "comun.h"
#include "definiciones.h" // Añadido para MAX_PARADAS

// Variables globales
int llega10 = 0;
int colagrafica;
int num_paradas_revisor;

// Variables globales para la lógica del revisor
static int parada_llegada_revisor;
static int parada_salida_revisor;
// static int num_pasajeros_simulado; // Ya no se usa, se leerá de la pipe
volatile sig_atomic_t flag_subir_bus = 0;
volatile sig_atomic_t flag_bajar_bus = 0;
static int fd_pipe_lectura_conteo = -1; // Para leer el conteo de pasajeros del bus


// Prototipos de funciones
void R10();
void R_alarma_revisor();
void visualiza_revisor(int cola, int parada, int inout, int pintaborra, int destino);
void R_subir_bus();
void R_bajar_bus();

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());

    // Configurar señales
    signal(10, R10); // Usamos 10 directamente para consistencia con cliente.c
    signal(SIGALRM, R_alarma_revisor);
    signal(12, R_subir_bus); // Señal del bus para que el revisor suba
    signal(16, R_bajar_bus); // Señal del bus para que el revisor baje

    // Obtener cola de mensajes del servidor gráfico
    colagrafica = crea_cola(ftok("./fichcola.txt", 18));
    if (colagrafica == -1) {
        perror("Revisor: Error al crear la cola de mensajes");
        exit(EXIT_FAILURE);
    }

    // Leer el número de paradas del argumento de línea de comandos
    if (argc > 1) {
        num_paradas_revisor = atoi(argv[1]);
        if (num_paradas_revisor <= 0 || num_paradas_revisor > MAXPARADAS) {
            fprintf(stderr, "Revisor %d: Número de paradas inválido (%s). Usando valor por defecto 6.\n", getpid(), argv[1]);
            num_paradas_revisor = 6;
        }
    } else {
        fprintf(stderr, "Revisor %d: No se recibió el número de paradas. Usando valor por defecto 6.\n", getpid());
        num_paradas_revisor = 6; // Valor por defecto
    }

    // Leer el descriptor de fichero de la pipe de conteo
    if (argc > 2) {
        fd_pipe_lectura_conteo = atoi(argv[2]);
        printf("Revisor %d: Pipe de lectura para conteo: FD %d.\n", getpid(), fd_pipe_lectura_conteo);
    } else {
        fprintf(stderr, "Revisor %d: Error, no se recibió el fd de la pipe de conteo. El conteo real no funcionará.\n", getpid());
        // fd_pipe_lectura_conteo permanecerá en -1
    }

    // Calcular tiempo de aparición aleatorio
    int tiempo_aparicion = rand() % 6 + 10; // Entre 10 y 15 segundos
    printf("Revisor %d (PID: %d) aparecerá en %d segundos.\n", getpid(), getpid(), tiempo_aparicion);
    alarm(tiempo_aparicion);

    pause(); // Espera a la alarma para la primera aparición (R_alarma_revisor se ejecuta)

    printf("Revisor %d esperando para subir al bus en parada %d.\n", getpid(), parada_llegada_revisor);
    // Esperar a subir
    while (!flag_subir_bus) {
        pause();
    }

    // Esperar a bajar
    printf("Revisor %d en el bus, esperando para bajar en parada %d.\n", getpid(), parada_salida_revisor);
    while (!flag_bajar_bus) {
        pause();
    }

    printf("Revisor %d ha completado su ciclo y termina.\n", getpid());
    return 0;
}

void R10() {
    // Manejador para la señal 10 (SIGUSR1) del servidor gráfico
    llega10 = 1;
}

void R_alarma_revisor() {
    // Lógica cuando el revisor "aparece"
    parada_llegada_revisor = rand() % num_paradas_revisor + 1;
    // Asegurar que la parada de salida sea diferente de la de llegada
    do {
        parada_salida_revisor = rand() % num_paradas_revisor + 1;
    } while (parada_salida_revisor == parada_llegada_revisor);


    printf("Revisor %d aparece en parada %d y saldrá en %d.\n", getpid(), parada_llegada_revisor, parada_salida_revisor);

    visualiza_revisor(colagrafica, parada_llegada_revisor, IN, PINTAR, parada_salida_revisor);
    // No hay pause() aquí, main maneja las esperas.
}

void R_subir_bus() {
    flag_subir_bus = 1; // Avisa a main para que deje de esperar esta señal

    printf("Revisor %d recibiendo señal para subir al bus desde parada %d.\n", getpid(), parada_llegada_revisor);
    // 1. Borrarse de la parada de llegada
    visualiza_revisor(colagrafica, parada_llegada_revisor, IN, BORRAR, parada_salida_revisor);

    // 2. Pintarse en el bus (parada 0)
    // Para el revisor en el bus, inout podría ser 0 y destino la parada de bajada.
    visualiza_revisor(colagrafica, 0, 0, PINTAR, parada_salida_revisor);

    // 3. Leer conteo de pasajeros de la pipe y escribir en fichero
    int pasajeros_leidos = -1; // Valor por defecto en caso de error
    if (fd_pipe_lectura_conteo != -1) {
        printf("Revisor %d: Intentando leer de la pipe FD %d.\n", getpid(), fd_pipe_lectura_conteo);
        if (read(fd_pipe_lectura_conteo, &pasajeros_leidos, sizeof(int)) == sizeof(int)) {
            printf("Revisor %d: Leyó %d pasajeros de la pipe.\n", getpid(), pasajeros_leidos);
        } else {
            perror("Revisor: Error al leer conteo de pasajeros de la pipe");
            // pasajeros_leidos permanecerá en -1 o el valor que tuviera
        }
        close(fd_pipe_lectura_conteo); // Cerrar la pipe después de leer
        fd_pipe_lectura_conteo = -1;   // Marcar como no usable
    } else {
        fprintf(stderr, "Revisor %d: Pipe de conteo no válida (FD %d), no se puede leer el conteo real.\n", getpid(), fd_pipe_lectura_conteo);
    }

    FILE *f_revisor = fopen("revisor.txt", "a"); // Abrir en modo append
    if (f_revisor == NULL) {
        perror("Revisor: Error al abrir revisor.txt");
    } else {
        fprintf(f_revisor, "El revisor %d comprueba que en la parada %d hay montados %d pasajeros\n",
                getpid(), parada_llegada_revisor, pasajeros_leidos); // Usar pasajeros_leidos
        fclose(f_revisor);
        printf("Revisor %d escribió en revisor.txt (Parada: %d, Pasajeros leídos: %d).\n", getpid(), parada_llegada_revisor, pasajeros_leidos);
    }
    // No poner pause() aquí, la espera la gestiona el bucle en main.
}

void R_bajar_bus() {
    flag_bajar_bus = 1; // Avisa a main para que deje de esperar esta señal

    printf("Revisor %d recibiendo señal para bajar del bus en parada %d.\n", getpid(), parada_salida_revisor);
    // 1. Borrarse del bus (parada 0)
    visualiza_revisor(colagrafica, 0, 0, BORRAR, parada_salida_revisor);

    // 2. Pintarse en la parada de salida (OUT)
    // Al bajar, el destino podría ser 0 o no relevante.
    visualiza_revisor(colagrafica, parada_salida_revisor, OUT, PINTAR, 0);

    printf("Revisor %d se ha bajado en parada %d.\n", getpid(), parada_salida_revisor);
    // No poner pause() aquí, la espera la gestiona el bucle en main, que luego terminará.
}


void visualiza_revisor(int cola, int parada, int inout, int pintaborra, int destino) {
    struct tipo_elemento peticion;

    peticion.tipo = 3; // REVISOR
    peticion.pid = getpid();
    peticion.parada = parada;
    peticion.inout = inout;
    peticion.pintaborra = pintaborra;
    peticion.destino = destino;

    if (msgsnd(cola, (struct tipo_elemento *)&peticion, sizeof(struct tipo_elemento) - sizeof(long), 0) == -1) {
        perror("Revisor: Error al enviar a la cola de mensajes del servidor");
    }

    // Esperar confirmación del servidor gráfico solo si estamos pintando o borrando
    // ya que el servidor gráfico siempre manda señal de confirmación
    if (pintaborra == PINTAR || pintaborra == BORRAR) {
        llega10 = 0;
        if (!llega10) {
            pause();
        }
        llega10 = 0;
    }
}
