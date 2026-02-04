#!/bin/bash

./basm.exe -o o.b -f $1
clear
./block -r -d -f o.b