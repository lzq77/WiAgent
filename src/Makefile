# build wimaster executable when user executes "make"

DEPS = controller_event.h
OBJS = main.o controller_event.o

LIBS += -levent

all: wimaster

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

wimaster: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

# remove object files and executable when user executes "make clean"
clean:
	rm -f *.o wimaster

.PHONY : clean
