# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -I./include -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -lpthread -lrt

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Target executable
TARGET = sync

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful! Executable: $(TARGET)"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@rm -rf local_docs/
	@echo "Cleaned build artifacts and local documents"

# Clean system resources (shared memory and message queues)
clean-resources:
	@rm -f /dev/shm/synctext_registry 2>/dev/null || true
	@rm -f /dev/mqueue/queue_user_* 2>/dev/null || true
	@echo "Cleaned system resources"

# Full clean
distclean: clean clean-resources

# Install dependencies (if needed)
install-deps:
	@echo "Installing dependencies..."
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y build-essential; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum groupinstall -y "Development Tools"; \
	elif command -v brew >/dev/null 2>&1; then \
		brew install gcc; \
	else \
		echo "Please install build tools manually"; \
	fi

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Test run with multiple users
test: $(TARGET)
	@echo "Starting test with 3 users..."
	@echo "Use Ctrl+C to stop all processes"
	@./$(TARGET) 1 & ./$(TARGET) 2 & ./$(TARGET) 3 & ./$(TARGET) 4 & ./$(TARGET) 5 & wait

# Phony targets
.PHONY: all clean clean-resources distclean install-deps debug test