#!/bin/bash

#step 1
zippedFile=$"openssl-1.0.1f.tar.gz"
file=$"openssl-1.0.1f"

if [ ! -f $zippedFile ];
	then 
		curl -O https://ftp.openssl.org/source/old/1.0.1/openssl-1.0.1f.tar.gz
	fi

if [ ! -f $file ];
	then
		tar xf openssl-1.0.1f.tar.gz
	fi

#step 2
cd openssl-1.0.1f/

if [ ! -f $"libcrypto.a" ];
	then
		./config
		make CC="clang -g -fsanitize=address,fuzzer-no-link"
	fi

cd ..

#step 3
if [ ! -f $"handshake-fuzzer.cc" ];
	then
		wget https://raw.githubusercontent.com/google/clusterfuzz/master/docs/setting-up-fuzzing/heartbleed/handshake-fuzzer.cc
	fi

if [ ! -f $"heandshake-fuzzer" ];
	then
		clang++ -g handshake-fuzzer.cc -fsanitize=address,fuzzer \
		openssl-1.0.1f/libssl.a openssl-1.0.1f/libcrypto.a \
		-std=c++17 -Iopenssl-1.0.1f/include/ -lstdc++fs -ldl -lstdc++ \
		-o handshake-fuzzer
	fi

#step 4
key=$"server.key"
pem=$"server.pem"

if [ ! -f $key ];
	then
		wget https://raw.githubusercontent.com/google/clusterfuzz/master/docs/setting-up-fuzzing/heartbleed/server.key
	fi

if [ ! -f $pem ];
	then
		wget https://raw.githubusercontent.com/google/clusterfuzz/master/docs/setting-up-fuzzing/heartbleed/server.pem
	fi

if [ -z $1 ];
	then
		./handshake-fuzzer
else
	./handshake-fuzzer $@
fi

exit 0
