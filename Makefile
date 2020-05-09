# GCC 4.9+
CC = gcc

# userctl options
CFLAGS += -O2 -Wall -Wextra -Wformat -Werror=implicit-function-declaration -Wformat-security -Werror=format-security -fstack-protector-strong -pedantic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LIBS += -lsystemd -pthread
INCLUDE += -Iinclude
SRCDIR = src
OBJDIR = obj
SRC = $(wildcard $(SRCDIR)/*.c)
# FIXME: When userctl only uses dbus to talk to userctld, want to remove classparser.o
USERCTL_OBJ = $(OBJDIR)/userctl.o $(OBJDIR)/utils.o $(OBJDIR)/commands.o $(OBJDIR)/vector.o
USERCTLD_OBJ = $(OBJDIR)/userctld.o $(OBJDIR)/classparser.o $(OBJDIR)/utils.o $(OBJDIR)/controller.o $(OBJDIR)/vector.o

.PHONY: all clean

all: userctl userctld

userctl: $(USERCTL_OBJ)
	$(CC) -o $@ $(USERCTL_OBJ) $(LIBS)

userctld: $(USERCTLD_OBJ)
	$(CC) -o $@ $(USERCTLD_OBJ) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJDIR)/* userctl userctld
