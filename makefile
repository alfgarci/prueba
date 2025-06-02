# Variables
CC = gcc
CFLAGS = -Wall -g -I. 
LDFLAGS = 
# For libraries like math (-lm) if needed by any component in the future.
# For now, only ncurses is explicitly handled for servidor_ncurses.
LIBS = 
NCURSES_LIB = -lncurses

# List of all target executables
TARGETS = principal bus cliente revisor servidor_ncurses

# Default rule: builds all targets
all: $(TARGETS)

# Linking rules for executables
principal: principal.o comun.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

bus: bus.o comun.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

cliente: cliente.o comun.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

revisor: revisor.o comun.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

servidor_ncurses: servidor_ncurses.o comun.o
	$(CC) $(LDFLAGS) $^ -o $@ $(NCURSES_LIB) $(LIBS)

# Compilation rules for object files
principal.o: principal.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

bus.o: bus.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

cliente.o: cliente.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

revisor.o: revisor.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

servidor_ncurses.o: servidor_ncurses.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

comun.o: comun.c comun.h definiciones.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f *.o $(TARGETS) fichcola.txt revisor.txt core.* vgcore.*

# Phony targets
.PHONY: all clean
