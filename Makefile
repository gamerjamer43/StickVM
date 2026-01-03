# all commands
.PHONY: all clean run

# change to clang if u rly want
CC := gcc

# compiler flags
FLAGS := -std=c99 -Wall -Wextra -O3 -g -fno-common -I. -Ivm -Ireader

# linker flags
LDFLAGS :=

# gather all .c files as a source
SRC := $(wildcard *.c */*.c)
OBJS := $(SRC:.c=.o)
DEPS := $(OBJS:.o=.d)

# vm.exe on windows
ifeq ($(OS),Windows_NT)
TARGET := vm.exe
RUN := .\$(TARGET)
RM := del /Q

# vm elf binary on linux
else
TARGET := vm
RUN := ./$(TARGET)
RM := rm -f
endif

# all compiles and runs (aka just make)
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# clean purges it all
clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

# run just runs assuming already build
run: all
	$(RUN)
