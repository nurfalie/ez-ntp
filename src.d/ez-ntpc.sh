#!/bin/sh

rc=0

case "$1" in
    start)
        if [ -e /var/run/ez-ntpc.pid ]
	then
    	    echo "The EzNTP client daemon may already be running."
    	    rc=1
	else
	    echo "Starting the EzNTP client daemon."
	    /usr/local/bin/ez-ntpc -h 192.168.178.1 -p 50000
	    rc=$?
	fi

	;;
    stop)
        if [ ! -e /var/run/ez-ntpc.pid ]
	then
	    echo "The EzNTP client daemon is not running."
	    rc=1
	else
	    echo "Terminating the EzNTP client daemon."
	    kill -TERM `cat /var/run/ez-ntpc.pid`
	    rc=$?
	fi

        ;;
    *)
	echo "Usage: /etc/init.d/ez-ntpc.sh {start|stop}"
	rc=1
	;;
esac

exit $rc
