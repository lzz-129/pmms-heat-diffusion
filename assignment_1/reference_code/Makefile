CURR_DIR=$(notdir $(basename $(shell pwd)))
PRJ=$(CURR_DIR)
REF=ref1.c ref2.c

SRC=$(filter-out $(REF), $(wildcard *.c))
OBJ=$(patsubst %.c,%.o,$(SRC))

CC=gcc
INCLUDES=-I../../include
ifndef DEBUG
CFLAGS=-O2 -std=gnu99 -lm
LIB=
else
CFLAGS=-O0 -g3 -std=c99
LIB=
endif

all: $(PRJ)

$(PRJ): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJ) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ $(LIB)	

clean:
	-rm -f $(OBJ) $(PRJ)
