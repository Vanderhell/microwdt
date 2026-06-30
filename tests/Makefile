CC ?= cc
CPPFLAGS += -I../include
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-qual -Wwrite-strings -Wformat=2 -Werror
LDFLAGS +=
LDLIBS +=

SRC = ../src/mwdt.c
TEST_SRC = test_all.c
TARGET = test_all

.PHONY: all clean

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRC) $(TEST_SRC) ../include/mwdt.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(TEST_SRC) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET) $(TARGET).exe test_all.out
