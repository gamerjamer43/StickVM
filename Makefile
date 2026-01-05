# all commands
.DEFAULT_GOAL := all
.PHONY: all clean run

CC := gcc

# compiler flags
FLAGS := -std=c99 -Wall -Wextra -O3 -g -fno-common -I. -Ivm -Iio

LDFLAGS :=

SRC  := $(wildcard *.c */*.c)
OBJS := $(SRC:.c=.o)
DEPS := $(OBJS:.o=.d)

ifeq ($(OS),Windows_NT)
TARGET := vm.exe
RUN    := .\$(TARGET)

# properly convert folder paths so make dont think theyre flags
WIN_CLEAN := $(subst /,\,$(OBJS) $(DEPS) $(TARGET))

clean:
	-@del /Q $(WIN_CLEAN) 2>NUL

else
TARGET := vm
RUN    := ./$(TARGET)

clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)
endif

# default rm for non-windows
RM := rm -f

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: all
	$(RUN)