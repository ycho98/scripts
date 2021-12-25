#!/bin/bash

# echo error message if no argument is passed
if [ -z $1 ]
then
	echo "Invalid argument"
	exit 255
fi

# build first argument
#clang++ -fsanitize=address,fuzzer $1 -o /home/ycho/scripts/hw3/clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-20.04.tar.xz

#build .cpp to .exe
clang++ -fsanitize=address,fuzzer $1 -o ${1%.*}.exe
myFile=${1%.*}

shift
./${myFile}.exe $@

exit 0


