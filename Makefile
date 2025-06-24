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

# Instalar dependências (Ubuntu/Debian)
install:
	sudo apt-get update
	sudo apt-get install build-essential libgtk-3-dev pkg-config glade

# Verificar se as dependências estão instaladas
check:
	@echo "Verificando dependências..."
	@pkg-config --exists gtk+-3.0 && echo "GTK+ 3.0: OK" || echo "GTK+ 3.0: FALTANDO"
	@which glade > /dev/null && echo "Glade: OK" || echo "Glade: FALTANDO"

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