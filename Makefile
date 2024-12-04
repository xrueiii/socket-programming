all: client.cpp
	g++ -std=c++11 client.cpp -o client

clean:
	rm -f client