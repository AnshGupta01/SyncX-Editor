CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g -pthread
LDFLAGS = -pthread -lrt

SRCDIR = src
BUILDDIR = build
INCDIR = include
TARGET = sync

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
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
	rm -f /dev/shm/synctext_registry /dev/mqueue/queue_user_*
	rm -rf local_docs/*.txt