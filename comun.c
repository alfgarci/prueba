#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For getpid(), pause()
// <sys/msg.h> and <sys/types.h> are included via comun.h
#include "comun.h"
#include "definiciones.h" // For TIPO_BUS, TIPO_CLIENTE, TIPO_REVISOR

/*********** FUNCION: crea_cola ********************************************************/ 
/*********** Obtiene acceso a la cola de mensajes con el id que se pasa ****************/
int crea_cola(key_t clave)
{
 int identificador;
 if(clave == (key_t) -1) 
 {
   printf("Error al obtener clave para cola mensajes\n");
   exit(-1);
 }

 identificador = msgget(clave, 0600 | IPC_CREAT);
 if (identificador == -1)
 {
   printf("Error al obtener identificador para cola mensajes\n");
   perror("msgget");
   exit (-1);
 }
 
 return identificador;
}

/*********** FUNCION: visualiza ******************************************************/
/* Envía un mensaje al servidor gráfico para pintar/borrar una entidad y espera    */
/* confirmación (SIGUSR1) si es una operación de pintado.                         */
/************************************************************************************/
// Parámetros: id_cola, tipo_de_entidad, parada_actual, si_entra_o_sale (IN/OUT), 
//             si_pinta_o_borra (PINTAR/BORRAR), parada_destino
void visualiza(int id_cola, int tipo_entidad, int parada_actual, int in_out_val, int pintar_borrar_val, int parada_destino) {
    struct tipo_elemento msg;
    pid_t pid_llamador = getpid();

    msg.tipo = (long)tipo_entidad; // TIPO_BUS, TIPO_CLIENTE, TIPO_REVISOR
    msg.pid = pid_llamador;
    msg.parada = parada_actual;
    msg.inout = in_out_val;             // IN (1) o OUT (0) según comun.h
    msg.pintaborra = pintar_borrar_val; // PINTAR (1) o BORRAR (0) según comun.h
    msg.destino = parada_destino;

    if (msgsnd(id_cola, &msg, sizeof(struct tipo_elemento) - sizeof(long), 0) == -1) {
        perror("visualiza: Error al enviar mensaje a la cola");
        // Considerar cómo manejar este error; salir podría ser demasiado drástico
        // para una función de librería. Por ahora, solo reporta.
    }

    // Si es una operación de PINTAR, esperar la señal SIGUSR1 del servidor gráfico.
    // Los procesos llamadores (cliente, bus, revisor) deben tener un manejador para SIGUSR1.
    if (pintar_borrar_val == PINTAR) { // PINTAR = 1 según comun.h
        pause(); 
    }
}
