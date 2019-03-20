# (C) 2016-2019 by www.vanheusden.com
VERSION=1.0

DEBUG=-g -pedantic #-pg #-fprofile-arcs
LDFLAGS+=$(DEBUG) -pthread -std=c++0x -flto
CXXFLAGS+=-O3 -Wall -DVERSION=\"$(VERSION)\" $(DEBUG) -std=c++0x -flto -fomit-frame-pointer -march=native

OBJS=error.o font.o main.o source.o text.o utils-gfx.o

all: pixelating

pixelating: $(OBJS)
	$(CXX) -Wall $(OBJS) $(LDFLAGS) -o pixelating

install: pixelating
	cp pixelating $(DESTDIR)/usr/local/bin

uninstall: clean
	rm -f $(DESTDIR)/usr/local/bin/pixelating

clean:
	rm -f $(OBJS) pixelating core gmon.out *.da
