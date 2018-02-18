CC=g++

all: proxy client

proxy: server.cpp
	$(CC) -o proxy server.cpp -lpthread
client: client.cpp
	$(CC) -o client client.cpp

.PHONY: clean

clean:
	rm -f proxy client