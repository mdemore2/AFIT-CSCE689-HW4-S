#!/bin/bash
gnome-terminal -e ./src/repsvr –p 9999 –t 8.0 –o svr1.txt –v 3 ./data/SingleDroneN1.bin
gnome-terminal -e ./src/repsvr -p 9998 –t 8.0 –o svr3.txt –v 3 ./data/SingleDroneN3.bin

