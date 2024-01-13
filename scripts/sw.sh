#!/bin/sh
progdir=`dirname "$0"`
case "$1" in
	1)
		rm -f $progdir/../iotester.sh
		cp -f $progdir/IO测试.sh $progdir/../
		sync
		;;
	2)
		rm -f $progdir/../IO测试.sh
		cp -f $progdir/iotester.sh $progdir/../
		sync
		;;
esac
