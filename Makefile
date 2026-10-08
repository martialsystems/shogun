CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Iengine

GRAPHFORGE_SRC ?= $(HOME)/graphforge/src
JUCE_SRC ?= $(HOME)/jidai-collection/jidai-rack/build/fl-release/_deps/juce-src

.PHONY: test clean asan law calib plugin plugin-linux probe-linux install-vst web web-levels

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

ENGINE_DEPS = engine/shogun.cpp $(wildcard engine/*.h engine/*.inc engine/voices/*.h engine/jidai/dsp/*.h)
TEST_SRCS = tests/main.cpp tests/blocks.cpp tests/voices.cpp tests/engine.cpp
TEST_DEPS = $(TEST_SRCS) tests/testutil.h tests/rig.h tests/data/levelcomp_cases.h

build/shogun_tests: $(ENGINE_DEPS) $(TEST_DEPS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Itests -o $@ engine/shogun.cpp $(TEST_SRCS)

asan: $(ENGINE_DEPS) $(TEST_DEPS)
	mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iengine -Itests \
		-o build/shogun_tests_asan engine/shogun.cpp $(TEST_SRCS)
	./build/shogun_tests_asan

# Calibration probe (§9.5, §15.5 step 1): prints calib = target peak / measured noon peak per voice.
# `make calib` rewrites engine/calib_table.inc from a calib = 1 build.
build/measure_calib: $(ENGINE_DEPS) tools/measure_calib.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tools/measure_calib.cpp engine/shogun.cpp

calib: build/measure_calib
	./build/measure_calib table > engine/calib_table.inc.new && mv engine/calib_table.inc.new engine/calib_table.inc
	./build/measure_calib

# The web page: the engine compiled to wasm (clang with the wasm32 target and wasm-ld, no libc),
# checked sample by sample against the native build, then inlined into web/shogun.html.
WASM_FLAGS = --target=wasm32 -std=c++17 -O2 -nostdlib -nostdinc -isystem web/wasm/include -Iengine \
	-fno-exceptions -fno-rtti -fno-builtin -Wall -Wextra -Werror \
	-Wl,--no-entry -Wl,--allow-undefined -Wl,-z,stack-size=262144

build/shogun.wasm: $(ENGINE_DEPS) web/wasm/shogun_web.cpp web/wasm/include/cmath web/wasm/include/cstdint
	mkdir -p build
	clang++ $(WASM_FLAGS) -o $@ engine/shogun.cpp web/wasm/shogun_web.cpp

build/web_parity: $(ENGINE_DEPS) web/wasm/shogun_web.cpp tests/web_parity.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ engine/shogun.cpp web/wasm/shogun_web.cpp tests/web_parity.cpp

web: build/shogun.wasm build/web_parity
	node web/test_wasm.mjs
	node web/build_page.mjs

# Re-measure the LEVEL table in web/page/kits.js after a kit or engine change, then make web.
web-levels: build/shogun.wasm
	node web/make_levels.mjs

clean:
	rm -rf build
