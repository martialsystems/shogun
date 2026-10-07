CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Iengine

.PHONY: test clean asan

test: build/shogun_tests
	./build/shogun_tests

build/shogun_tests: engine/shogun.cpp engine/shogun.h engine/dsp.h tests/voices.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ engine/shogun.cpp tests/voices.cpp

asan: engine/shogun.cpp engine/shogun.h engine/dsp.h tests/voices.cpp
	mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -Iengine \
		-o build/shogun_tests_asan engine/shogun.cpp tests/voices.cpp
	./build/shogun_tests_asan

clean:
	rm -rf build
