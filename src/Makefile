
CC=gcc
CX=g++
CFLAGS=-W -Wall -O3 -pedantic -fomit-frame-pointer


all: simfunge.exe fungasm.exe bef2elf.exe elf2ppm.exe

simfunge.exe: simmain.o fungus.o uint18.o felfin.o fungdis.o
	$(CX) $(CFLAGS) $^ -o $@

fungasm.exe: asmmain.o felfout.o fungdis.o getline.o fungasm.o asmcmnt.o
	$(CX) $(CFLAGS) $^ -o $@

bef2elf.exe: bef2elf.o felfout.o
	$(CX) $(CFLAGS) $^ -o $@

elf2ppm.exe: elf2ppm.o felfin.o ImageFmtc.o
	$(CX) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

%.o: %.cc
	$(CX) $(CFLAGS) $^ -c -o $@

