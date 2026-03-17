#!/bin/bash

./basm.exe -o o.b -f $1
./block -r -f o.b