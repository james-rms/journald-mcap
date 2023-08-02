CC := g++ -std=c++17 -Wall -Werror

bin/journal2mcap: src/main.cpp
	mkdir -p bin
	$(CC) -o $@ $? -lsystemd -lzstd -llz4 -Iinclude

build/fake-systemd.o: test/fake_systemd.cpp
	mkdir -p build
	$(CC) -o $@ -c $?

bin/tests: test/test.cpp include/cmdline.hpp include/journal.hpp build/fake-systemd.o
	mkdir -p bin
	$(CC) -o $@ test/test.cpp build/fake-systemd.o -Iinclude

.PHONY = test
test: bin/tests
	$?
