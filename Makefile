CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Iengine

GRAPHFORGE_SRC ?= $(HOME)/graphforge/src
JUCE_SRC ?= $(HOME)/jidai-collection/jidai-rack/build/fl-release/_deps/juce-src

.PHONY: test clean asan law plugin install-vst

test: build/shogun_tests law
	./build/shogun_tests

law:
	PYTHONPATH="$(GRAPHFORGE_SRC)" python3 forge/tests/test_switch_law.py

plugin:
	cmake -S plugin -B build/plugin -G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
		-DFETCHCONTENT_SOURCE_DIR_JUCE="$(JUCE_SRC)"
	cmake --build build/plugin --target Shogun_VST3 ShogunProbe -j 8
	./build/plugin/ShogunProbe_artefacts/Release/ShogunProbe /tmp/shogun-plate.png
	codesign --force --sign - --timestamp=none \
		build/plugin/Shogun_artefacts/Release/VST3/SHOGUN.vst3

install-vst: plugin
	mkdir -p "$(HOME)/Library/Audio/Plug-Ins/VST3"
	ln -sfn "$(CURDIR)/build/plugin/Shogun_artefacts/Release/VST3/SHOGUN.vst3" \
		"$(HOME)/Library/Audio/Plug-Ins/VST3/SHOGUN.vst3"

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
