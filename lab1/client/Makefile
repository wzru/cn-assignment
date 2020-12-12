tftp-client: tftp.h logger.h tftp-client.cpp
	clang++ tftp-client.cpp -o tftp-client -g -O2 -std=c++20 -Wall

get: tftp-client
	./tftp-client -m netascii get 1.txt

put: tftp-client
	./tftp-client -m etascii put Makefile

clean:
	rm -f 1.txt