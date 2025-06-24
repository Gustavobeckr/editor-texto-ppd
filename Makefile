# Makefile para programa GTK + MPI

# Nome do executável
TARGET = edito_texto_ppd

# Arquivo fonte
SRC = main.c

# Compilador MPI
CC = mpicc

# Flags do compilador para GTK
CFLAGS = `pkg-config --cflags gtk+-3.0` -Wall -pthread
LIBS = `pkg-config --libs gtk+-3.0` -pthread

# Regra principal
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

# Regra para limpar arquivos compilados
clean:
	rm -f $(TARGET)

# Regra para executar com 1 processo (programa GTK simples)
run: $(TARGET)
	mpirun -np 2 ./$(TARGET)


# Regra para executar apenas localmente sem mpirun
run-local: $(TARGET)
	./$(TARGET)

.PHONY: clean run run-mpi run-local