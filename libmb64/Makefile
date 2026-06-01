CC     ?= gcc
CFLAGS += -Wall -Wextra -std=c11 -O2 -I.
PYTHON ?= python3

SRC    := mb64.c mb64_mesh.c
OBJ    := $(SRC:.c=.o)
LIB    := libmb64.a
THEME_DATA := mb64_theme_data.generated.inc.c

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^
	@echo "[LIBMB64] built $@ from $^"

$(THEME_DATA): ../tools/gen_libmb64_theme_data.py ../src/mb64/data.c ../include/surface_terrains.h
	$(PYTHON) ../tools/gen_libmb64_theme_data.py \
		--data-c ../src/mb64/data.c \
		--surface-terrains ../include/surface_terrains.h \
		--out $@

mb64_mesh.o: $(THEME_DATA) mb64_tile_types.h

%.o: %.c mb64.h mb64_log.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB) $(THEME_DATA)
