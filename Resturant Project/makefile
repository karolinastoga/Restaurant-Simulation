all: cli td kd server

cli: client.o
	gcc -Wall client.o -o cli

td: table.o
	gcc -Wall table.o -o td

kd: kitchen-device.o
	gcc -Wall kitchen-device.o -o kd

server: server.o
	gcc -Wall server.o -o server -pthread

clean:
	rm -f *.o cli td kd server
