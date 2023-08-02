CC := g++ -std=c++17 -Wall -Werror

bin/journal2mcap: src/main.cpp src/cmdline.cpp src/journal.cpp
	mkdir -p bin
	$(CC) -o $@ $^ -lsystemd -lzstd -llz4 -Iinclude

bin/tests: test/test.cpp src/cmdline.cpp src/journal.cpp test/fake_systemd.cpp
	mkdir -p bin
	$(CC) -o $@ $^ -Iinclude

.PHONY: test
test: bin/tests
	$^
