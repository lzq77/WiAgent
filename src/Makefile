# build wimaster executable when user executes "make"

all: wimaster
LIBS += -levent

wimaster: main.o
	$(CC) main.o -o wimaster $(LDFLAGS) $(LIBS)
main.o: main.c
	$(CC) -c main.c $(CFLAGS)

# remove object files and executable when user executes "make clean"
clean:
	rm *.o wimaster
