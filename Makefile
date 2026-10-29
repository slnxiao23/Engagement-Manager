CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -I src

.PHONY: all test clean
all: build/demo build/tests

build:
	mkdir -p build

build/demo: src/main.cpp | build
	$(CXX) $(CXXFLAGS) src/main.cpp -o build/demo

build/tests: tests/engagement_manager_test.cpp src/main.cpp doctest/doctest.h | build
	$(CXX) $(CXXFLAGS) tests/engagement_manager_test.cpp -o build/tests

test: build/tests
	./build/tests

clean:
	rm -rf build
