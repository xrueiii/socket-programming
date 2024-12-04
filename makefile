all: server.cpp
	g++ -std=c++11 server.cpp -o server

clean:
	rm -f server
