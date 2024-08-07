# Nome do compilador
CC = gcc
NAME = lcmc20-fvq21
# Opções de compilação
CFLAGS = -Wall -Wextra -std=c99 -g

# Nome do executável
EXE = fodinha

# Arquivos de código-fonte
SRC = main.c connection.c

# Regra padrão
all: $(EXE)

# Regra para criar o executável
$(EXE): $(SRC)
	$(CC) $(CFLAGS) -o $(EXE) $(SRC)

connection.o: connection.c connection.h
	$(CC) $(CFLAGS) -c connection.c

# Limpar arquivos gerados
clean:
	rm -f $(EXE)

compact:
	tar -cvzf $(NAME).tar.gz *.c *.h Makefile Relatório.pdf

