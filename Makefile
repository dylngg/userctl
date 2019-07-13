# GCC 4.9+
CC = gcc

# userctl options
CFLAGS += -std=c99 -O2 -Wall -Wextra -Wformat -Werror=implicit-function-declaration -Wformat-security -Werror=format-security -fstack-protector-strong -pedantic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
INCLUDE += -Iinclude
EXE = userctl
SRCDIR = src
OBJDIR = obj
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) -o $@ $(OBJ)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ)
