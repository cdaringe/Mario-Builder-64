CC     ?= gcc
CFLAGS += -Wall -Wextra -std=c11 -O2 -I.

SRC    := mb64.c mb64_mesh.c
OBJ    := $(SRC:.c=.o)
LIB    := libmb64.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^
	@echo "[LIBMB64] built $@ from $^"

%.o: %.c mb64.h mb64_log.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB)
