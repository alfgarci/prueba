#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include <unistd.h>


#include "definiciones.h"
#include "comun.h"

// Color pair definitions for Revisor (global to this file)
#define COLOR_PAIR_REVISOR_LLEGADA 7
#define COLOR_PAIR_REVISOR_BUS_SALIDA 8

/***** PROTOTIPOS DE FUNCONES *******************/ 
void pinta_escenario();

void inserta(struct cliente laventana[][MAXCLIENTES], int parada, int elpid, int destino);
void quita(struct cliente laventana[][MAXCLIENTES], int parada, int elpid);
void visualiza_peticion(struct tipo_elemento peticion);
void pinta_clientes_inverso(WINDOW *ventana, struct cliente datos_ventana[][MAXCLIENTES], int parada);
void pinta_clientes(WINDOW *ventana, struct cliente datos_ventana[][MAXCLIENTES], int parada);

void limpia_array(struct cliente datos[MAXPARADAS][MAXCLIENTES]);
void R12();


/***** DECLARACION DE VARIABLES GLOBALES ***********/
 
int fin=0;
int ultimaparada=6;

WINDOW *vparadain[MAXPARADAS];
WINDOW *vparadaout[MAXPARADAS];
WINDOW *vmensajes;
WINDOW *vcarretera;

 
/**********************************************************************************/
/************       MAIN        ***************************************************/
/**********************************************************************************/


int main(int argc, char *argv[])
{



 struct cliente datos_paradain[MAXPARADAS][MAXCLIENTES];
 struct cliente datos_paradaout[MAXPARADAS][MAXCLIENTES];

 struct tipo_elemento peticion;
 key_t Clave1; 
 int Id_Cola_Mensajes;



 // Nos preparamos para recibir la senyal 12
 signal(12,R12);
 
 //recogemos el argumento que nos pasan con la ultima parada
 if (argc==2) ultimaparada=atoi(argv[1]);



 //Limpiamos todas los arrays
 limpia_array(datos_paradain);
 limpia_array(datos_paradaout);


 // Dibujamos el escenario
 pinta_escenario();


 // Creamos y abrimos la cola de mensajes
 Clave1 = ftok ("./fichcola.txt", 18);
 Id_Cola_Mensajes=crea_cola(Clave1);

 
 kill(getppid(),10); // avisamos a principal de que todo va bien para continuar

 msgrcv(Id_Cola_Mensajes, (struct tipo_elemento *) &peticion,sizeof(struct tipo_elemento)-sizeof(long), 0, 0);     
 while(!fin) // Esto acaba cuando llegue la senyal 12
 {
   usleep(RETARDO);
   //espero que lleguen peticiones a la cola de mensajes

   
   // Visualizo en la zona de mensajes la informacon sobre la peticion
   visualiza_peticion(peticion);

  //decodifico y ejecuto la peticion sobre los arrays
  if(peticion.tipo==2){ // Son peticiones de clientes
	if(peticion.pintaborra==PINTAR)
  {
		if(peticion.inout==IN) inserta(datos_paradain,peticion.parada,peticion.pid,peticion.destino);
		else inserta(datos_paradaout,peticion.parada,peticion.pid,peticion.destino);
  }
	if(peticion.pintaborra==BORRAR){
		if(peticion.inout==IN) quita(datos_paradain,peticion.parada,peticion.pid);
		else quita(datos_paradaout,peticion.parada,peticion.pid);
  }

	if(peticion.inout==IN) pinta_clientes_inverso(vparadain[peticion.parada],datos_paradain,peticion.parada);
	else pinta_clientes(vparadaout[peticion.parada],datos_paradaout,peticion.parada);
  } // Fin de TIPO_CLIENTE (peticion.tipo == 2)
  else if (peticion.tipo == TIPO_REVISOR) { 
      WINDOW *target_window = NULL;
      int color_pair_id = 0; 
      char display_text[25]; 

      // visualiza_peticion() ya fue llamada antes del if/else if.
      // peticion.tipo, peticion.pid, peticion.parada, peticion.inout, 
      // peticion.pintaborra, peticion.destino están disponibles.
      // TIPO_REVISOR (3), PINTAR (1), BORRAR (0), IN (1), OUT (0)
      // COLOR_PAIR_REVISOR_LLEGADA (7 - Green/Black)
      // COLOR_PAIR_REVISOR_BUS_SALIDA (8 - Yellow/Black)

      if (peticion.parada == 0) { // Revisor en el bus
          target_window = vparadaout[0]; // vparadaout[0] es la ventana del bus
          color_pair_id = COLOR_PAIR_REVISOR_BUS_SALIDA;
      } else if (peticion.parada > 0 && peticion.parada <= ultimaparada) { // Revisor en una parada válida
          if (peticion.inout == IN) { // Llegando a la parada
              target_window = vparadain[peticion.parada];
              color_pair_id = COLOR_PAIR_REVISOR_LLEGADA;
          } else { // OUT - Saliendo de la parada (o ya en la zona de salida)
              target_window = vparadaout[peticion.parada];
              color_pair_id = COLOR_PAIR_REVISOR_BUS_SALIDA;
          }
      }

      if (target_window) {
          // El revisor se mostrará en la primera línea de la ventana correspondiente.
          wmove(target_window, 0, 0); // Mover cursor a la primera línea, primera columna
          wclrtoeol(target_window);   // Limpiar desde el cursor hasta el final de la línea

          if (peticion.pintaborra == PINTAR) {
              // Formato: R<PID_MOD_100>-><DESTINO_PARADA>
              sprintf(display_text, "R%02d->%d", peticion.pid % 100, peticion.destino);
              wattron(target_window, COLOR_PAIR(color_pair_id));
              // Escribir en la primera línea, columna 1 (para un pequeño margen)
              mvwprintw(target_window, 0, 1, "%s", display_text); 
              wattroff(target_window, COLOR_PAIR(color_pair_id));
              
              // Enviar señal SIGUSR1 (10) al revisor para confirmar pintado
              if (kill(peticion.pid, 10) == -1) {
                  // Opcional: manejar error si kill falla, ej. log a vmensajes
                  // perror("Servidor_ncurses: kill SIGUSR1 a revisor falló");
                  // mvwprintw(vmensajes, 0,0, "Error kill R%d", peticion.pid); wrefresh(vmensajes);
              }
          }
          // Si es BORRAR, la línea ya fue limpiada por wclrtoeol.
          wrefresh(target_window);
      }
  } // Fin de TIPO_REVISOR
  else { //son peticiones del autobús	(asumiendo TIPO_BUS = 1, o cualquier tipo no cliente y no revisor)
	werase(vcarretera);
	if(peticion.parada<10) { // son las paradas
		mvwprintw(vcarretera,0,ANCHO*3*(peticion.parada-1)+ANCHO+ANCHO/2,"_____ ");
		mvwprintw(vcarretera,1,ANCHO*3*(peticion.parada-1)+ANCHO+ANCHO/2,"|- - \\");		
		mvwprintw(vcarretera,2,ANCHO*3*(peticion.parada-1)+ANCHO+ANCHO/2,"######");		
		mvwprintw(vcarretera,3,ANCHO*3*(peticion.parada-1)+ANCHO+ANCHO/2," O  O ");		

	}
    else {// son los viajes entre paradas
		mvwprintw(vcarretera,1,ANCHO*3*(peticion.parada%10-1),"_____ ");
		mvwprintw(vcarretera,2,ANCHO*3*(peticion.parada%10-1),"|- - \\");		
		mvwprintw(vcarretera,3,ANCHO*3*(peticion.parada%10-1),"######");		
		mvwprintw(vcarretera,4,ANCHO*3*(peticion.parada%10-1)," O  O");		
	}
    wrefresh(vcarretera);
    kill(peticion.pid,10); //le digo al proceso que puede continuar  
  } 
  msgrcv(Id_Cola_Mensajes, (struct tipo_elemento *) &peticion,sizeof(struct tipo_elemento)-sizeof(long), 0, 0);     
 
 }
   
 endwin(); //Finaliza el uso de ncurses
 msgctl (Id_Cola_Mensajes, IPC_RMID, 0); //Borra la cola de mensajes

}


/*********** FUNCION: inserta **********************************************************/ 
/*********** Inserta un pid en el array si hay sitio, sino mata la proceso *************/
void inserta(struct cliente laventana[][MAXCLIENTES], int parada, int elpid, int destino)
{
 int i=0;

 while(laventana[parada][i].elpid!=0 && i<MAXCLIENTES) i++;

 if(i==MAXCLIENTES) 
  //kill(elpid,12); //mato al proceso porque no se puede representar
  printf("No es posible incluir el proceso en el array ... desbordamiento\n"); else
 {
  laventana[parada][i].elpid=elpid;
  laventana[parada][i].destino= destino;
  kill(elpid,10); //le digo al proceso que puede continuar
 }
}


/********** FUNCION: elimina ***********************************************************/
/********** Elimina un pid de un array y lo reordena  *********************************/
void quita(struct cliente laventana[][MAXCLIENTES], int parada, int elpid)
{
 int i=0;
 int j;
 
 while(laventana[parada][i].elpid!=elpid && i<MAXCLIENTES) i++;

 if(i==MAXCLIENTES) printf("Error, se intenta borrar un pid que no esta");
 else{ 
  for(j=i;j<MAXCLIENTES-1;j++)
  {
   laventana[parada][j].elpid=laventana[parada][j+1].elpid;
   laventana[parada][j].destino=laventana[parada][j+1].destino;
  }
  laventana[parada][j].elpid=0;
  laventana[parada][j].destino=0;
 }
}



/*********** FUNCION: pinta_escenario ***************************************************/
/*********** Dibuja el escenario *******************************************************/
void pinta_escenario()
{
 int i;
 initscr(); // INICIALIZA NCURSES
 start_color(); //INICIALIZA COLORES

 
  
 // ESTO COMPRUEBA SI EL TERMINAL SOPORTA EL USO DE COLORES, SINO ACABA
 if (!has_colors()){
   endwin();
   printf("Esta terminal no tiene colores\n");
   kill(getppid(),12); //mata al principal que espera conformidad con la 10
   exit(-1);
 }
 //COMPROBAMOS QUE TENEMOS LAS LINEAS Y COLUMNAS QUE NECESITAMOS
 if(LINES<40 || COLS<120)
 {
   endwin();
   printf("Se necesitan, al menos 40 lineas y 120 columnas, y tienes %d lineas y %d columnas\n",LINES,COLS);
   kill(getppid(),12); //mata al principal que espera conformidad con la 10
   exit(-1);
 }

  //VISUALIZAMOS LA RESOLUCI ACUTAL DEL TERMINAL
  //printw("La pantalla tiene %d lineas y %d columnas)",LINES, COLS);

  //Ponemos la cabecera
  attron(A_BOLD);
  printw("       . Parada 1 .      . Parada 2 .      . Parada 3 .      . Parada 4 .      . Parada 5 .      . Parada 6 .");
  refresh();

 // definimos los pares de colores
   init_pair(COLOR_PARADAIN,COLOR_WHITE,COLOR_BLUE);
   init_pair(COLOR_PARADAOUT,COLOR_RED,COLOR_WHITE);
   init_pair(COLOR_BUS,COLOR_WHITE,COLOR_CYAN);
   init_pair(COLOR_FONDO,COLOR_WHITE,COLOR_BLACK);
   init_pair(COLOR_DIBUJOBUS,COLOR_YELLOW,COLOR_BLACK);
   init_pair(COLOR_ACERA,COLOR_YELLOW,COLOR_BLUE);
   // New color pairs for Revisor
   init_pair(COLOR_PAIR_REVISOR_LLEGADA, COLOR_GREEN, COLOR_BLACK);
   init_pair(COLOR_PAIR_REVISOR_BUS_SALIDA, COLOR_YELLOW, COLOR_BLACK);

 
//***********


 vparadaout[0]=newwin(6,ANCHO*4,ALTO+7,30);
 wbkgd(vparadaout[0],COLOR_PAIR(COLOR_BUS));
 wattron(vparadaout[0],A_BOLD);
 wrefresh(vparadaout[0]);

 vparadain[1]=newwin(ALTO,ANCHO,1,ANCHO*1+1);
 wbkgd(vparadain[1],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[1],A_BOLD);
 wrefresh(vparadain[1]);

 vparadaout[1]=newwin(ALTO,ANCHO,1,ANCHO*2+1);
 wbkgd(vparadaout[1],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[1],A_BOLD);
 wrefresh(vparadaout[1]);

 vparadain[2]=newwin(ALTO,ANCHO,1,ANCHO*4+1);
 wbkgd(vparadain[2],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[2],A_BOLD);
 wrefresh(vparadain[2]);

 vparadaout[2]=newwin(ALTO,ANCHO,1,ANCHO*5+1);
 wbkgd(vparadaout[2],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[2],A_BOLD);
 wrefresh(vparadaout[2]);

 vparadain[3]=newwin(ALTO,ANCHO,1,ANCHO*7+1);
 wbkgd(vparadain[3],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[3],A_BOLD);
 wrefresh(vparadain[3]);

 vparadaout[3]=newwin(ALTO,ANCHO,1,ANCHO*8+1);
 wbkgd(vparadaout[3],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[3],A_BOLD);
 wrefresh(vparadaout[3]);

 vparadain[4]=newwin(ALTO,ANCHO,1,ANCHO*10+1);
 wbkgd(vparadain[4],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[4],A_BOLD);
 wrefresh(vparadain[4]);

 vparadaout[4]=newwin(ALTO,ANCHO,1,ANCHO*11+1);
 wbkgd(vparadaout[4],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[4],A_BOLD);
 wrefresh(vparadaout[4]);

 vparadain[5]=newwin(ALTO,ANCHO,1,ANCHO*13+1);
 wbkgd(vparadain[5],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[5],A_BOLD);
 wrefresh(vparadain[5]);

 vparadaout[5]=newwin(ALTO,ANCHO,1,ANCHO*14+1);
 wbkgd(vparadaout[5],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[5],A_BOLD);
 wrefresh(vparadaout[5]);

 vparadain[6]=newwin(ALTO,ANCHO,1,ANCHO*16+1);
 wbkgd(vparadain[6],COLOR_PAIR(COLOR_PARADAIN));
 wattron(vparadain[6],A_BOLD);
 wrefresh(vparadain[6]);

 vparadaout[6]=newwin(ALTO,ANCHO,1,ANCHO*17+1);
 wbkgd(vparadaout[6],COLOR_PAIR(COLOR_PARADAOUT));
 wattron(vparadaout[6],A_BOLD);
 wrefresh(vparadaout[6]);

 //Esta será la acera
 vparadaout[7]=newwin(8,ANCHO*8,ALTO+7,60);
 wbkgd(vparadaout[7],COLOR_PAIR(COLOR_ACERA));
 wattron(vparadaout[7],A_BOLD);
 wrefresh(vparadaout[7]);


 vmensajes=newwin(6,24,ALTO+7,1);
 wbkgd(vmensajes,COLOR_PAIR(COLOR_FONDO));
 wattron(vmensajes,A_BOLD);
 wrefresh(vmensajes);

 vcarretera=newwin(5,ANCHO*3*(MAXPARADAS-1),ALTO+1,1);
 wbkgd(vcarretera,COLOR_PAIR(COLOR_DIBUJOBUS));
 wattron(vcarretera,A_BOLD);
 wrefresh(vcarretera);

 // OCULTAMOS LAS QUE NO SE USAN
 for(i=ultimaparada+1; i<MAXPARADAS-1; i++)
 {
	wbkgd(vparadain[i],COLOR_PAIR(COLOR_FONDO));
	werase(vparadain[i]);
	wrefresh(vparadain[i]);	
	wbkgd(vparadaout[i],COLOR_PAIR(COLOR_FONDO));
	werase(vparadaout[i]);
	wrefresh(vparadaout[i]);	
 }


}                               


/************** FUNCION: visualiza_peticion  ******************************************************/
/************** Visualiza las peticiones recibidas en la ventana de mensajes *********************/

void visualiza_peticion(struct tipo_elemento peticion)
{    
  werase(vmensajes);
  wprintw(vmensajes,"Recibido mensaje:\n");
  wprintw(vmensajes,"\t tipo: %ld\n", peticion.tipo);
  wprintw(vmensajes,"\t pid: %d\n", peticion.pid);
  wprintw(vmensajes,"\t parada: %d\n", peticion.parada);
  wprintw(vmensajes,"\t in-out: %d\n",peticion.inout);
  wprintw(vmensajes,"\t operacion: %d\n",peticion.pintaborra);
  wprintw(vmensajes,"\t destino: %d\n",peticion.destino);
  wrefresh(vmensajes);
}


/************** FUNCION: pinta_clientes **********************************************************/
/************** Pinta los clientes en la ventana que indiquemos *********************************/


void pinta_clientes(WINDOW *ventana, struct cliente datos_ventana[][MAXCLIENTES], int parada)
{
    int i;
   
    werase(ventana);
	
	if(parada==0) wattron(ventana,COLOR_PAIR(COLOR_BUS));
	else if (parada==7) wattron(ventana,COLOR_PAIR(COLOR_ACERA));
		 else  wattron(ventana,COLOR_PAIR(COLOR_PARADAOUT));

	if(parada==7) 
	{
    	for(i=0;i<MAXCLIENTES;i++) 
     		if(datos_ventana[parada][i].elpid!=0)
	 		{
		 		wprintw(ventana," %02d-%d ",datos_ventana[parada][i].elpid%100,datos_ventana[parada][i].destino);
	 		}
	}
	else
	{ 
    	for(i=0;i<ALTO;i++) 
     		if(datos_ventana[parada][i].elpid!=0)
	 		{
		 		wprintw(ventana," %02d-%d ",datos_ventana[parada][i].elpid%100,datos_ventana[parada][i].destino);
	 		}
	}
    wrefresh(ventana);
}


/************** FUNCION: pinta_clientes inverso****************************************************/
/************** Pinta los clientes en la ventana que indiquemos del último al primero*************/

void pinta_clientes_inverso(WINDOW *ventana, struct cliente datos_ventana[][MAXCLIENTES], int parada)
{
    int j;

    werase(ventana);
    //wprintw(ventana,"\n");
	wattron(ventana,COLOR_PAIR(COLOR_PARADAIN));

     for(j=ALTO-1; j>=0; j--)
      if(datos_ventana[parada][j].elpid!=0)
		 wprintw(ventana," %02d-%d\n",datos_ventana[parada][j].elpid%100,datos_ventana[parada][j].destino);
		else wprintw(ventana,"\n");
    wrefresh(ventana);
}


/************** FUNCION: limpia_array **********************************************************/
/************** Limpia los campos del array que indiquemos ************************************/
void limpia_array(struct cliente datos[MAXPARADAS][MAXCLIENTES])
{
 int i,j;
   for(i=0;i<MAXPARADAS;i++)
		for(j=0;j<MAXCLIENTES;j++) {
	      datos[i][j].elpid=0; 
    	  datos[i][j].destino=-1;
   }
}


/************** FUNCION: R12 *******************************************************************/
/************** Atiende a la senyal 12 cuando llega *******************************************/
void R12()
{
 fin=1; //cuando llega la senyal 12 finalizo el servidor.
}
