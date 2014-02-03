#!/bin/sh

case "$1" in
start)
        if [ -e /var/run/ez-ntpc.pid ]
	then
    		echo "The EzNTP client daemon may already be running."
    		exit 1
	fi

	echo "Starting the EzNTP client daemon."
	/usr/local/bin/ez-ntpc -p 50000 -h 192.168.178.1
	;;
stop)
        if [ ! -e /var/run/ez-ntpc.pid ]
	then
	        echo "The EzNTP client daemon is not running."
		exit 1
	else
	        echo "Terminating the EzNTP client daemon."
	        kill -TERM `cat /var/run/ez-ntpc.pid`
	fi

        ;;
*)
	echo "Usage: /etc/init.d/ez-ntpc.sh {start|stop}"
	exit 1
	;;
esac

exit 0
