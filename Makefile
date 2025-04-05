CC := gcc
CFLAGS := -g -I./include
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

# Default target
all: a.out

# Compile main application as a.out
a.out: $(MAIN_OBJ) $(OBJ_DIR)/src/alarm.o $(OBJ_DIR)/src/display.o $(OBJ_DIR)/src/console.o $(OBJ_DIR)/src/circular_buffer.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile main object file
$(MAIN_OBJ): $(MAIN_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile source files
$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -rf $(OBJ_DIR) a.out

# Phony targets
.PHONY: all clean
