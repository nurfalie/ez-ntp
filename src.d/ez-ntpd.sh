#!/bin/sh

rc=0

case "$1" in
    start)
        if [ -e /var/run/ez-ntpd.pid ]
	then
    	    echo "The EzNTP daemon may already be running."
    	    rc=1
	else
	    echo "Starting the EzNTP daemon."
	    /usr/local/bin/ez-ntpd
	    rc=$?
	fi

	;;
    stop)
        if [ ! -e /var/run/ez-ntpd.pid ]
	then
	    echo "The EzNTP daemon is not running."
	    rc=1
	else
	    echo "Terminating the EzNTP daemon."
	    kill -TERM `cat /var/run/ez-ntpd.pid`
	    rc=$?
	fi

        ;;
    *)
	echo "Usage: /etc/init.d/ez-ntpd.sh {start|stop}"
	rc=1
	;;
esac

exit $rc
