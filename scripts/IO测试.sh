#!/bin/sh
progdir=$(dirname "$0")/iotester
cd $progdir
HOME=$progdir
LD_PRELOAD=./j2k.so ./iotester.elf
sync
