SOURCES=buddy.c show.c
OBJECTS:=$(addsuffix .o,$(basename $(SOURCES)))
SHOW_BIN=show

CWARNINGFLAGS=-Wformat -Wno-format-extra-args -Wformat-security \
	-Wwrite-strings -Wcast-qual -Wconversion -Wshadow -Wextra
LDFLAGS=-g
CFLAGS=-Wall -std=gnu99 -g $(CWARNINGFLAGS)
CC=gcc

all: compile

.PHONY: all run compile clean

run: compile
	./$(SHOW_BIN)
	
compile: $(SHOW_BIN)
	
$(SHOW_BIN): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
buddy.o: buddy.c buddy.h
show.o: show.c buddy.h

clean:
	rm -f *.o $(SHOW_BIN)
