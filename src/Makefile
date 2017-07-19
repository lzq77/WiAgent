# build wimaster executable when user executes "make"

DEPS = controller_event.h controller_push.h
OBJS = main.o controller_event.o controller_push.o

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
