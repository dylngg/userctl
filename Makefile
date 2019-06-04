CC = gcc

# userctl options
CFLAGS += -std=c99 -Wall -Wextra -Wformat -Wformat-security -Werror=format-security -Werror=implicit-function-declaration -fstack-protector -pedantic -D_FORTIFY_SOURCE=2
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
