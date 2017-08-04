AKEFLAGS += --no-print-directory

PREFIX ?= /usr
SBINDIR ?= $(PREFIX)/sbin
MANDIR ?= $(PREFIX)/share/man
PKG_CONFIG ?= pkg-config

MKDIR ?= mkdir -p
INSTALL ?= install
CC ?= "gcc"

CFLAGS ?= -MMD -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common

LIBS += -levent -lnl-tiny -ljson-c

OBJS += ./main.o
OBJS += ./utils/common.o
OBJS += ./agent/handler.o
OBJS += ./agent/push.o
OBJS += ./ap/config_file.o
OBJS += ./ap/hostapd.o
OBJS += ./ap/beacon.o
OBJS += ./ap/hw_features.o
OBJS += ./ap/hostapd_mgmt.o
OBJS += ./ap/wi_vap.o

-include $(OBJS:%.o=%.d)

all: wimaster

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

wimaster: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o wimaster

.PHONY : clean
