CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c11 -Iheaders -Iparsing/headers_pars
LDFLAGS = -lrt

# Tutti i sorgenti tranne client
EXEC_SRC = $(wildcard exec/*.c)
PARSING_SRC = $(wildcard parsing/*.c)
SRC = main.c $(EXEC_SRC) $(PARSING_SRC)
OBJ = $(SRC:.c=.o)

# Header
HEADERS = $(wildcard headers/*.h) $(wildcard parsing/headers_pars/*.h)

# Client separato
CLIENT_SRC = client.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
# Usa gli oggetti già generati da exec/ (niente duplicati)
CLIENT_DEPS = exec/logger.o parsing/parse_env.o exec/utils.o

# Binarî finali
BIN = emergenza
CLIENT_BIN = client

.PHONY: all clean run

all: logdir $(BIN) $(CLIENT_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJ) $(CLIENT_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

logdir:
	@mkdir -p log

run: all
	./$(BIN)

clean:
	rm -f $(OBJ) $(CLIENT_OBJ) $(BIN) $(CLIENT_BIN)
