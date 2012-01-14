all: font.cpp main.c
	g++ `sdl-config --libs --cflags` -L/lib/security -L/usr/local/lib -L/lib -L/usr/lib -lpam font.cpp main.c -o sdldm

clean: *.o  sdldm
	rm -rf *.o sdldm
