CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g -pthread

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    LDFLAGS = -pthread -lrt
else
    LDFLAGS = -pthread
endif


SRCDIR = src
BUILDDIR = build
TARGET = sync_text

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SRC))

.PHONY: all clean setup

all: setup $(TARGET)

setup:
	@mkdir -p $(BUILDDIR)
	@mkdir -p local_docs

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
	rm -f /tmp/user_*.sock
	rm -f /dev/shm/synctext_registry
	rm -rf local_docs/*.txt