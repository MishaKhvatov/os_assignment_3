# Compiler and flags
CC := gcc
CFLAGS := -Wall -Werror -g -I./include
LDFLAGS := -lpthread

# Directories
SRC_DIR := src
INCLUDE_DIR := include
TEST_DIR := test
OBJ_DIR := obj

# Create directories if they don't exist
$(shell mkdir -p $(OBJ_DIR)/src $(OBJ_DIR)/test)

# Source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(SRC_FILES))

# Main application
MAIN_SRC := New_Alarm_Cond.c
MAIN_OBJ := $(OBJ_DIR)/New_Alarm_Cond.o
MAIN_BIN := New_Alarm_Cond

# Test files
TEST_BINS := alarm_test display_test start_test

# Default target
all: $(MAIN_BIN) $(TEST_BINS)

# Compile main application
$(MAIN_BIN): $(MAIN_OBJ) $(OBJ_DIR)/src/alarm.o $(OBJ_DIR)/src/display.o $(OBJ_DIR)/src/console.o $(OBJ_DIR)/src/circular_buffer.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile main object file
$(MAIN_OBJ): $(MAIN_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile source files
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile and link display_test
display_test: $(TEST_DIR)/display_test.c $(OBJ_DIR)/src/display.o $(OBJ_DIR)/src/alarm.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile and link alarm_test
alarm_test: $(TEST_DIR)/alarm_test.c $(OBJ_DIR)/src/alarm.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile and link start_test
start_test: $(TEST_DIR)/start_test.c $(OBJ_DIR)/src/alarm.o $(OBJ_DIR)/src/display.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Run all tests
run-tests: $(TEST_BINS)
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "\nRunning $$test"; \
		./$$test; \
	done

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(MAIN_BIN) $(TEST_BINS)

# Phony targets
.PHONY: all clean run-tests
