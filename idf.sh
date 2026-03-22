#!/bin/bash

if [ $# == 0 ]
then 
	echo "To enter './idf.sh H' get help"
	exit
fi

if [ $1 = "I" ]
then
	echo "idf.py set-target esp32s3"
	idf.py set-target esp32s3

elif [ $1 = "M" ]
then
	echo "idf.py menuconfig"
	idf.py menuconfig

elif [ $1 = "B" ]
then
	echo "chmod 777 /dev/ttyACM0"
	sudo chmod 777 /dev/ttyACM0
	echo "idf.py -p /dev/ttyACM0 flash"
	idf.py -p /dev/ttyACM0 flash

elif [ $1 = "BM" ]
then
	echo "chmod 777 /dev/ttyACM0"
	sudo chmod 777 /dev/ttyACM0
	echo "idf.py -p /dev/ttyACM0 flash monitor"
	idf.py -p /dev/ttyACM0 flash monitor

elif [ $1 = "m" ]
then
	echo "chmod 777 /dev/ttyACM0"
	sudo chmod 777 /dev/ttyACM0
	echo "idf.py -p /dev/ttyACM0 monitor"
	idf.py -p /dev/ttyACM0 monitor

elif [ $1 = "C" ]
then
	echo "idf.py create-component -C $3 $2"
	idf.py create-component -C $3 $2

elif [ $1 == "clean" ]
then 
	echo "clean"
	idf.py clean

elif [ $1 = "H" ]
then
	echo "I		set-target esp32s3"
	echo "M		menuconfig"
	echo "B		build and flash"
	echo "BM		build  flash and monitor"
	echo "C		create-component <component name> <component path>  "
	echo "H		help"

fi
