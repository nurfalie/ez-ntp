GCC_OPTIONS	= -Wall -Werror -Wextra -Wformat -Wpointer-arith \
		  -Wstack-protector -Wstrict-overflow=5 \
		  -Wstrict-prototypes \
		  -fPIE -fstack-protector-all -pedantic -pie
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
	SYSTEM=osx
else ifeq ($(UNAME), FreeBSD)
	SYSTEM=freebsd
else ifeq ($(UNAME), Linux)
	SYSTEM=linux
else
	SYSTEM=linux
endif

all:
	$(MAKE) -f Makefile.$(SYSTEM).client
	$(MAKE) -f Makefile.$(SYSTEM).daemon

clean:
	$(MAKE) -f Makefile.$(SYSTEM).client clean
	$(MAKE) -f Makefile.$(SYSTEM).daemon clean
	rm -f core ez-ntpc.core ez-ntpd.core

distclean: clean purge

install:
	$(MAKE) -f Makefile.$(SYSTEM).client install
	$(MAKE) -f Makefile.$(SYSTEM).daemon install

purge:
	rm -f *~
