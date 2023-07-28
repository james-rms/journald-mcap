CC := g++ -std=c++17 -Wall -Werror

bin/journal2mcap: src/main.cpp
	mkdir -p bin
	$(CC) -o $@ $? -lsystemd -lzstd -llz4 -Iinclude
