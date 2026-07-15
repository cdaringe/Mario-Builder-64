CC     ?= gcc
AR     ?= ar
CFLAGS += -Wall -Wextra -std=c11 -O2 -I.
PYTHON ?= python3

SRC    := mb64.c mb64_mesh.c mb64_object_catalog.c
OBJ    := $(SRC:.c=.o)
LIB    := libmb64.a
THEME_DATA := mb64_theme_data.generated.inc.c
MESH_DATA := mb64_mesh_data.generated.inc.c
OBJECT_CATALOG_DATA := mb64_object_catalog.generated.inc.c
TEST_BINS := test_mesh_parity test_object_catalog

.PHONY: all clean test

all: $(LIB)

$(LIB): $(OBJ)
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(AR) rcs "$$tmp" $^; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM
	@echo "[LIBMB64] built $@ from $^"

$(THEME_DATA): ../tools/gen_libmb64_theme_data.py ../src/mb64/data.c ../include/surface_terrains.h
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(PYTHON) ../tools/gen_libmb64_theme_data.py \
		--data-c ../src/mb64/data.c \
		--surface-terrains ../include/surface_terrains.h \
		--out "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

$(MESH_DATA): ../tools/gen_libmb64_mesh_data.py ../src/mb64/data.c ../src/mb64/boundary.c
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(PYTHON) ../tools/gen_libmb64_mesh_data.py \
		--data-c ../src/mb64/data.c \
		--boundary-c ../src/mb64/boundary.c \
		--out "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

mb64_mesh.o: $(THEME_DATA) $(MESH_DATA) mb64_faceshapes.h mb64_tile_types.h

test_mesh_parity: tests/test_mesh_parity.c $(LIB) mb64.h mb64_object_types.h mb64_tile_types.h
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(CC) $(CFLAGS) tests/test_mesh_parity.c $(LIB) -o "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

test_object_catalog: tests/test_object_catalog.c $(LIB) mb64_object_catalog.h
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(CC) $(CFLAGS) tests/test_object_catalog.c $(LIB) -o "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

test: $(TEST_BINS)
	./test_mesh_parity
	./test_object_catalog

$(OBJECT_CATALOG_DATA): ../tools/gen_libmb64_object_catalog.py ../src/mb64/data.c mb64_object_types.h
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(PYTHON) ../tools/gen_libmb64_object_catalog.py \
		--data-c ../src/mb64/data.c \
		--object-types-h mb64_object_types.h \
		--out "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

mb64_object_catalog.o: $(OBJECT_CATALOG_DATA) mb64_object_catalog.h mb64_object_types.h

%.o: %.c mb64.h mb64_log.h
	@set -e; tmp="$@.tmp.$$$$"; \
	trap 'rm -f "$$tmp"' EXIT INT TERM; \
	$(CC) $(CFLAGS) -c $< -o "$$tmp"; \
	mv -f "$$tmp" "$@"; \
	trap - EXIT INT TERM

clean:
	rm -f $(OBJ) $(LIB) $(THEME_DATA) $(MESH_DATA) $(OBJECT_CATALOG_DATA) $(TEST_BINS)
