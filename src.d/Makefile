standard:
	$(MAKE) -f Makefile.client
	$(MAKE) -f Makefile.daemon

clean:
	$(MAKE) -f Makefile.client clean
	$(MAKE) -f Makefile.daemon clean
	/bin/rm -f ez-ntpc.core
	/bin/rm -f ez-ntpd.core
	/bin/rm -f core

purge:
	/bin/rm -f *~

install:
	$(MAKE) -f Makefile.client install
	$(MAKE) -f Makefile.daemon install
