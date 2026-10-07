CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Iengine

GRAPHFORGE_SRC ?= $(HOME)/graphforge/src

.PHONY: test clean asan law

test: build/shogun_tests law
	./build/shogun_tests

law:
	PYTHONPATH="$(GRAPHFORGE_SRC)" python3 forge/tests/test_switch_law.py

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
