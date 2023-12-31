#! /bin/sh

case "$1" in
	start)
		echo "starting aesdsocket"
		start-stop-daemon --start /usr/bin/aesdsocket -- -d
		;;
	stop)
		echo "stopping aesdsocket"
		start-stop-daemon -K -n aesdsocket
		;;
	*)
		echo "usage: $0 {start|stop}"
	exit 1
esac


exit 0
