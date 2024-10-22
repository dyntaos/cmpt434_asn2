###################################################
##            CMPT 434 - Assignment 2            ##
##          University of Saskatchewan           ##
##                     2020                      ##
##-----------------------------------------------##
##                  Kale Yuzik                   ##
##                kay851@usask.ca                ##
##      NSID: kay851     Student #11071571       ##
###################################################



CC = gcc
CFLAGS =
CPPFLAGS = -Wall -Wextra -pedantic -g
LDFLAGS =

ARCH = $(shell uname -s)$(shell uname -m)

BUILD = ./build
BIN = $(BUILD)/bin/$(ARCH)
OBJ = $(BUILD)/obj/$(ARCH)
LIB = $(BUILD)/lib/$(ARCH)

.PHONY: all mkdirs clean

all: mkdirs \
	$(BIN)/receiver \
	$(BIN)/sender \
	$(BIN)/forwarder


mkdirs:
	mkdir -p $(BIN) $(OBJ) $(LIB)

clean:
	rm -rf ./build \
		./receiver \
		./sender \
		./forwarder



$(OBJ)/udp.o: udp.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<



$(OBJ)/receiver.o: receiver.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BIN)/receiver: $(OBJ)/receiver.o $(OBJ)/udp.o
	$(CC) -o $@ $^
	ln -fs $@ ./receiver



$(OBJ)/sender.o: sender.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BIN)/sender: $(OBJ)/sender.o $(OBJ)/udp.o
	$(CC) -o $@ $^
	ln -fs $@ ./sender



$(OBJ)/forwarder.o: forwarder.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(BIN)/forwarder: $(OBJ)/forwarder.o $(OBJ)/udp.o
	$(CC) -o $@ $^
	ln -fs $@ ./forwarder

