standard:
	$(MAKE) -f Makefile.client
	$(MAKE) -f Makefile.daemon

clean:
	$(MAKE) -f Makefile.client clean
	$(MAKE) -f Makefile.daemon clean
	rm -f ez-ntpc.core
	rm -f ez-ntpd.core
	rm -f core

purge:
	rm -f *~

install:
	$(MAKE) -f Makefile.client install
	$(MAKE) -f Makefile.daemon install
