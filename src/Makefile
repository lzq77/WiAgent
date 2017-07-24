AKEFLAGS += --no-print-directory

PREFIX ?= /usr
SBINDIR ?= $(PREFIX)/sbin
MANDIR ?= $(PREFIX)/share/man
PKG_CONFIG ?= pkg-config

MKDIR ?= mkdir -p
INSTALL ?= install
CC ?= "gcc"

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common


LIBS += -levent -lnl-tiny -ljson-c

DEPS = ap_config.h beacon.h com.h config_file.h controller_event.h \
	   controller_push.h driver_nl80211.h hostapd.h hw_features.h \
	   ieee802_1x_defs.h nl80211_copy.h sta_info.h wpabuf.h

OBJS = main.o controller_event.o controller_push.o config_file.o com.o hostapd.o \
	   beacon.o hw_features.o 

all: wimaster

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

wimaster: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

# remove object files and executable when user executes "make clean"
clean:
	rm -f *.o wimaster

.PHONY : clean
