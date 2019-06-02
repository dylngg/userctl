CC = gcc

# userctl options
CFLAGS += -Wall -std=c99 -Wformat -Wformat-security -O2 -D_FORTIFY_SOURCE=2 -Werror=implicit-function-declaration
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
