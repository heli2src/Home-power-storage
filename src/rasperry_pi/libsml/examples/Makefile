UNAME := $(shell uname)
CFLAGS += -I../sml/include/ -I../../paho.mqtt.c/src/ -g -std=c99 -Wall -Wextra -pedantic
OBJS = sml_mqtt.o
LIBSML = ../sml/lib/libsml.a
LIBMQT = ../../paho.mqtt.c/build/output/libpaho-mqtt3c.so.1
INSTALL = /usr/bin/install
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include

LIBS = -lm
ifneq ($(UNAME), Darwin)
LIBS += -luuid
endif

all: sml_mqtt

sml_mqtt: $(OBJS) $(LIBSML)
	$(CC) $(OBJS) $(LIBSML) $(LIBS) $(LIBMQT) -o sml_mqtt	

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean install uninstall
clean:
	@rm -f *.o
	@rm -f sml_mqtt

install: sml_mqtt
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) sml_mqtt $(DESTDIR)$(bindir)

uninstall:
	@rm -f $(DESTDIR)$(bindir)/sml_mqtt
