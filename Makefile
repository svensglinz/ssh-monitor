CC=gcc
FLAGS=-O3 -Iinclude
SRC_DIR=./src

OBJS := $(wildcard $(SRC_DIR)/*.c)
OUTPUT= ./bin/ssh-monitor
LFLAGS=-lsqlite3 -lsystemd
all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@mkdir -p ./bin
	$(CC) $(FLAGS) $(OBJS) -o $@ $(LFLAGS)

clean:
	rm -f $(OUTPUT)