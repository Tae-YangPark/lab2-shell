CC ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
OBJ = src/shell.o src/parser.o src/directory_commands.o src/mkdir.o src/rmdir.o src/ln.o src/cp.o
.PHONY: all test clean
all: lab2-shell

lab2-shell: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/shell.o: include/commands.h include/parser.h
src/parser.o: include/parser.h
src/directory_commands.o: include/commands.h

test: lab2-shell
	python3 tests/smoke.py ./lab2-shell

clean:
	$(RM) lab2-shell $(OBJ)
