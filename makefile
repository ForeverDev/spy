CC = gcc
CF = -std=c99 -Wno-switch -O0 -g
OBJ = build/spyre.o build/main.o build/api.o build/assembler_lex.o build/assembler.o build/lex.o build/parse.o build/generate.o

all: spy.exe

clean:
	rm -Rf build

spy.exe: build $(OBJ)
	$(CC) $(CF) $(OBJ) -o spy.exe
	cp spy.exe C:\MinGW\bin\spy.exe
	rm spy.exe
	rm -Rf build

build:
	mkdir build

build/spyre.o: 
	$(CC) $(CF) -c spyre.c -o build/spyre.o

build/api.o:
	$(CC) $(CF) -c api.c -o build/api.o

build/assembler_lex.o:
	$(CC) $(CF) -c assembler_lex.c -o build/assembler_lex.o

build/assembler.o:
	$(CC) $(CF) -c assembler.c -o build/assembler.o

build/lex.o:
	$(CC) $(CF) -c lex.c -o build/lex.o

build/parse.o:
	$(CC) $(CF) -c parse.c -o build/parse.o

build/generate.o:
	$(CC) $(CF) -c generate.c -o build/generate.o

build/main.o:
	$(CC) $(CF) -c main.c -o build/main.o

