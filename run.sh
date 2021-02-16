#!/bin/bash

# Making sure a file has been passed.
if [ -z "$1" ]
then
	echo "Please specify files!"
	exit 1
fi

# Assuming that the current system is little endian.
echo "Little Endian:"
g++-10 -std=c++20 -fconcepts -O3 $1
./a.out

echo "--------------------------------------"
# mips architecture is big endian.
echo "Big Endian:"
mips-linux-gnu-g++-10 -std=c++20 -fconcepts -O3 -static $1
qemu-mips ./a.out

rm ./a.out
echo "--------------------------------------"
echo "Finished."
