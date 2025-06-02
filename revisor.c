#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
// Asume que estos archivos existen en la misma ruta o en una ruta de inclusión
#include "comun.h"
#include "definiciones.h"

// Variables Globales
pid_t g_pid_revisor;
int g_id_cola;
int g_parada_llegada;
int g_parada_salida;
volatile sig_atomic_t bus_llego_y_listo = 0;
int g_pipe_fd_lectura = -1; // Pipe para leer número de pasajeros del bus

// Prototipos de manejadores de señales
void manejador_sigint(int sig);
void manejador_sigusr1(int sig);
void manejador_sigusr2_bus_listo(int sig);

// Prototipo de la función para escribir en el fichero
void escribir_en_fichero_revisor(pid_t pid, int parada_subida, int num_pasajeros);


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <num_paradas> <id_cola> <pipe_fd_lectura>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_paradas = atoi(argv[1]);
    g_id_cola = atoi(argv[2]);
    g_pipe_fd_lectura = atoi(argv[3]);
    g_pid_revisor = getpid();

    printf("Proceso REVISOR %d iniciado (PID: %d, Paradas: %d, ColaID: %d, PipeReadFD: %d).\n", 
           g_pid_revisor, g_pid_revisor, num_paradas, g_id_cola, g_pipe_fd_lectura);

    // Manejadores de señales
    signal(SIGINT, manejador_sigint);
    signal(SIGUSR1, manejador_sigusr1);
    signal(SIGUSR2, manejador_sigusr2_bus_listo);

    // Inicializar semilla para números aleatorios
    srand(time(NULL) ^ g_pid_revisor);

    // 1. Inicio diferido (10-15 segundos)
    int tiempo_espera = (rand() % 6) + 10; // Genera entre 10 y 15
    printf("Revisor %d: Esperando %d segundos antes de aparecer...\n", g_pid_revisor, tiempo_espera);
    sleep(tiempo_espera);

    // 2. Selección de paradas
    if (num_paradas <= 0) {
        fprintf(stderr, "Revisor %d: Número de paradas debe ser positivo.\n", g_pid_revisor);
        exit(EXIT_FAILURE);
    }
    g_parada_llegada = (rand() % num_paradas) + 1;
    g_parada_salida = (g_parada_llegada % num_paradas) + 1;

    printf("Revisor %d: Aparece en parada %d. Se bajará en parada %d.\n", g_pid_revisor, g_parada_llegada, g_parada_salida);

    // Visualizarse en la parada de llegada
    printf("Revisor %d: Visualizando en parada %d (Destino: %d)...\n", g_pid_revisor, g_parada_llegada, g_parada_salida);
    visualiza(g_id_cola, TIPO_REVISOR, g_parada_llegada, IN, PINTAR, g_parada_salida);

    printf("Revisor %d: Esperando al autobús en la parada %d... (esperando SIGUSR2)\n", g_pid_revisor, g_parada_llegada);
    bus_llego_y_listo = 0; // Asegurarse que está en 0 antes de esperar
    while (!bus_llego_y_listo) {
        pause(); // Espera a cualquier señal
    }

    printf("Revisor %d: Bus ha llegado a la parada %d.\n", g_pid_revisor, g_parada_llegada);
    
    int num_pasajeros_en_bus = -1; // Valor por defecto
    if (g_pipe_fd_lectura != -1) {
        printf("Revisor %d: Leyendo número de pasajeros del pipe FD %d...\n", g_pid_revisor, g_pipe_fd_lectura);
        ssize_t bytes_leidos = read(g_pipe_fd_lectura, &num_pasajeros_en_bus, sizeof(int));
        if (bytes_leidos == sizeof(int)) {
            printf("Revisor %d: Leídos %d pasajeros del bus desde el pipe.\n", g_pid_revisor, num_pasajeros_en_bus);
        } else if (bytes_leidos == 0) {
            printf("Revisor %d: Pipe cerrado por el bus (EOF). No se leyeron datos de pasajeros. Usando %d.\n", g_pid_revisor, num_pasajeros_en_bus);
        } else { // bytes_leidos == -1
            perror("Revisor: Error al leer número de pasajeros del pipe");
            printf("Revisor %d: Usando %d pasajeros debido a error de lectura.\n", g_pid_revisor, num_pasajeros_en_bus);
        }
        // Cerrar el pipe después de la lectura (o intento de lectura)
        close(g_pipe_fd_lectura);
        g_pipe_fd_lectura = -1; 
    } else {
        printf("Revisor %d: Pipe no válido FD %d. No se puede leer número de pasajeros. Usando %d.\n", g_pid_revisor, g_pipe_fd_lectura, num_pasajeros_en_bus);
    }

    escribir_en_fichero_revisor(g_pid_revisor, g_parada_llegada, num_pasajeros_en_bus);

    printf("Revisor %d: Subiendo al bus...\n", g_pid_revisor);
    // Visualizar subida al bus
    visualiza(g_id_cola, TIPO_REVISOR, 0, IN, PINTAR, g_parada_salida); // Parada 0 es el bus

    printf("Revisor %d: En el bus, viajando hacia parada %d.\n", g_pid_revisor, g_parada_salida);
    sleep(4); // Simular tiempo de viaje (entre 3-5 segundos)

    // Visualizar bajada en destino
    printf("Revisor %d: Llegando a parada de destino %d. Bajando...\n", g_pid_revisor, g_parada_salida);
    visualiza(g_id_cola, TIPO_REVISOR, g_parada_salida, OUT, PINTAR, g_parada_salida);

    // Borrar de la parada de salida
    printf("Revisor %d: Borrando de la parada %d.\n", g_pid_revisor, g_parada_salida);
    visualiza(g_id_cola, TIPO_REVISOR, g_parada_salida, OUT, BORRAR, g_parada_salida);

    printf("Revisor %d: Tarea completada y bajado en destino.\n", g_pid_revisor);
    
    // Asegurarse que el pipe está cerrado si no se hizo antes (ej. si g_pipe_fd_lectura era -1 inicialmente)
    if (g_pipe_fd_lectura != -1) {
        close(g_pipe_fd_lectura);
        g_pipe_fd_lectura = -1;
    }
    return 0;
}

void manejador_sigint(int sig) {
    printf("Revisor %d: Terminando por señal %d (SIGINT).\n", g_pid_revisor, sig);
    if (g_pipe_fd_lectura != -1) {
        printf("Revisor %d: Cerrando pipe FD %d en SIGINT handler.\n", g_pid_revisor, g_pipe_fd_lectura);
        close(g_pipe_fd_lectura);
        g_pipe_fd_lectura = -1;
    }
    exit(EXIT_SUCCESS);
}

void manejador_sigusr1(int sig) {
    // Opcional: printf("Revisor %d: Señal SIGUSR1 %d recibida del servidor gráfico.\n", g_pid_revisor, sig);
}

void manejador_sigusr2_bus_listo(int sig) {
    // Opcional: printf("Revisor %d: Recibida señal SIGUSR2 (%d), bus listo.\n", g_pid_revisor, sig);
    bus_llego_y_listo = 1;
}

void escribir_en_fichero_revisor(pid_t pid, int parada_subida, int num_pasajeros) {
    FILE *f_revisor = fopen("revisor.txt", "a");
    if (f_revisor == NULL) {
        perror("Revisor: Error al abrir revisor.txt");
        return;
    }
    // Corregido: %M a %d para num_pasajeros
    fprintf(f_revisor, "El revisor %d comprueba que en la parada %d hay montados %d pasajeros\n", pid, parada_subida, num_pasajeros);
    fclose(f_revisor);
}
