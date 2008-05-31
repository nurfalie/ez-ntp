#!/bin/sh

case "$1" in
start)
        if [ -e /var/run/ez-ntpd.pid ]
	then
    		echo "The EzNTP daemon may already be running."
    		exit 1
	fi

	echo "Starting the EzNTP daemon."
	/usr/local/bin/ez-ntpd --disable_all_logs
	;;
stop)
        if [ ! -e /var/run/ez-ntpd.pid ]
	then
	        echo "The EzNTP daemon is not running."
		exit 1
	else
	        echo "Terminating the EzNTP daemon."
	        kill -TERM `cat /var/run/ez-ntpd.pid`
	fi

        ;;
*)
	echo "Usage: /etc/init.d/ez-ntpd.sh {start|stop}"
	exit 1
	;;
esac

exit 0
