CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
DEBUG_FLAGS = -g -O0
RELEASE_FLAGS = -O3

TARGET = donut.exe
SRC = donut.c

DEBUG_DIR = build/debug
RELEASE_DIR = build/release

DEBUG_TARGET = $(DEBUG_DIR)/$(TARGET)
RELEASE_TARGET = $(RELEASE_DIR)/$(TARGET)

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(SRC)
	if not exist "$(DEBUG_DIR)" mkdir "$(DEBUG_DIR)"
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(SRC) -o $(DEBUG_TARGET)

release: $(RELEASE_TARGET)

$(RELEASE_TARGET): $(SRC)
	if not exist "$(RELEASE_DIR)" mkdir "$(RELEASE_DIR)"
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(SRC) -o $(RELEASE_TARGET)

run: debug
	$(DEBUG_TARGET)

clean:
	if exist build rmdir /s /q build
