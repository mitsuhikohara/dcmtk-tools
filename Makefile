#
#	Makefile for dcmtk tools
#

SHELL = /bin/sh
configdir = config

include $(configdir)/Makefile.def

.NOTPARALLEL:

all:  config-all mppsscp-all storcmtscp-all

install:  config-install mppsscp-install storcmtscp-install

install-all: install

install-bin:  config-install-bin mppsscp-install-bin storcmtscp-install-bin

config-all:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" all)

config-libsrc-all:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" libsrc-all)

config-tests-all:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" tests-all)

config-install:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install)

config-install-bin:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-bin)

config-install-doc:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-doc)

config-install-data:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-data)

config-install-etc:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-etc)

config-install-lib:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-lib)

config-install-include:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-include)

config-install-support:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-support)

config-check:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" check)

config-check-exhaustive:
	(cd config && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" check-exhaustive)

mppsscp-all:
	(cd mppsscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" all)

mppsscp-install:
	(cd mppsscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install)

mppsscp-install-bin:
	(cd mppsscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-bin)

storcmtscp-all:
	(cd storcmtscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" all)

storcmtscp-install:
	(cd storcmtscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install)

storcmtscp-install-bin:
	(cd storcmtscp && $(MAKE) ARCH="$(ARCH)" DESTDIR="$(DESTDIR)" install-bin)

dependencies:
	-(cd config && $(MAKE) dependencies)
	(cd mppsscp && $(MAKE) dependencies)
	(cd storcmtscp && $(MAKE) dependencies)

clean:
	(cd mppsscp && $(MAKE) clean)
	(cd storcmtscp && $(MAKE) clean)
	-(cd config && $(MAKE) clean)
	rm -f $(TRASH)

distclean:
	(cd mppsscp && $(MAKE) distclean)
	(cd storcmtscp && $(MAKE) distclean)
	-(cd config && $(MAKE) distclean)
	rm -f $(TRASH)

