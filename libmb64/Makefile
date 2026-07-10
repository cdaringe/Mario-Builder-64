CC     ?= gcc
CFLAGS += -Wall -Wextra -std=c11 -O2 -I.
PYTHON ?= python3

SRC    := mb64.c mb64_mesh.c
OBJ    := $(SRC:.c=.o)
LIB    := libmb64.a
THEME_DATA := mb64_theme_data.generated.inc.c
MESH_DATA := mb64_mesh_data.generated.inc.c
TEST_BIN := test_mesh_parity

.PHONY: all clean test

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^
	@echo "[LIBMB64] built $@ from $^"

$(THEME_DATA): ../tools/gen_libmb64_theme_data.py ../src/mb64/data.c ../include/surface_terrains.h
	$(PYTHON) ../tools/gen_libmb64_theme_data.py \
		--data-c ../src/mb64/data.c \
		--surface-terrains ../include/surface_terrains.h \
		--out $@

$(MESH_DATA): ../tools/gen_libmb64_mesh_data.py ../src/mb64/data.c ../src/mb64/boundary.c
	$(PYTHON) ../tools/gen_libmb64_mesh_data.py \
		--data-c ../src/mb64/data.c \
		--boundary-c ../src/mb64/boundary.c \
		--out $@

mb64_mesh.o: $(THEME_DATA) $(MESH_DATA) mb64_faceshapes.h mb64_tile_types.h

$(TEST_BIN): tests/test_mesh_parity.c $(LIB) mb64.h mb64_object_types.h mb64_tile_types.h
	$(CC) $(CFLAGS) tests/test_mesh_parity.c $(LIB) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

%.o: %.c mb64.h mb64_log.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB) $(THEME_DATA) $(MESH_DATA) $(TEST_BIN)
