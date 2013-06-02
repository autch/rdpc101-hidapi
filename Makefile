CFLAGS=-I/usr/local/include -g -O2 -Wall
LDFLAGS=-L/usr/local/lib -lusb-1.0 -Wl,-rpath,/usr/local/lib
OBJS= rdpc101.o librdpc101.o

all: rdpc101

rdpc101: ${OBJS}

rdpc-test: rdpc-test.o
	cc -o $@ $< librdpc101.o ${LDFLAGS}
clean:
	rm -f rdpc101 *.o a.out
