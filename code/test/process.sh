#!/bin/bash

make clean && make fileIO_test1
../build.linux/nachos -e fileIO_test1