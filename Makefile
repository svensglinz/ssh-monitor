CC=gcc
FLAGS=-O3 -Iinclude
SRC_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))src

OBJS := $(wildcard $(SRC_DIR)/*.c)
OUTPUT=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))bin/ssh-monitor
LFLAGS=-lsqlite3 -lsystemd
all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@mkdir -p ./bin
	$(CC) $(FLAGS) $(OBJS) -o $@ $(LFLAGS)

clean:
	rm -f $(OUTPUT)