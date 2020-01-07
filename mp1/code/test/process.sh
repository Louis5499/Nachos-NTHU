#!/bin/bash

make clean && make fileIO_test2
../build.linux/nachos -e fileIO_test2 -d u