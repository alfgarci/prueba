all: principal servidor_ncurses cliente bus revisor

principal: principal.c comun.h comun.c
	cc principal.c comun.c -o principal -Wall

servidor_ncurses: servidor_ncurses.c comun.c definiciones.h comun.h
	cc servidor_ncurses.c comun.c -o servidor_ncurses -lncurses -Wall

cliente: cliente.c comun.c comun.h
	cc cliente.c comun.c -o cliente -Wall

bus: bus.c comun.c comun.h 
	cc bus.c comun.c -o bus -Wall

revisor: revisor.c comun.c comun.h definiciones.h
	cc revisor.c comun.c -o revisor -Wall

clean:
	rm -f principal servidor_ncurses cliente bus revisor *.o
