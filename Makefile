CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O2 -Iengine

# Shared Jidai headers (JCS + jidai::dsp), vendored from jidai-collection (third_party/jidai-common/VENDORED.md).
JIDAI_COMMON ?= third_party/jidai-common/include
JIDAI_INC = -I$(JIDAI_COMMON)

GRAPHFORGE_SRC ?= $(HOME)/graphforge/src
JUCE_SRC ?= $(HOME)/jidai-collection/jidai-rack/build/fl-release/_deps/juce-src

.PHONY: test panel-check clean asan law calib plugin plugin-linux probe-linux install-vst web web-levels strict strict-clang strict-gcc factory

test: build/shogun_tests panel-check law
	./build/shogun_tests

# No dead panel keys: every key / toggle / knob / jack in PanelLayout.inc is bound, handled and live.
panel-check:
	python3 scripts/check_panel_bindings.py

law:
	PYTHONPATH="$(GRAPHFORGE_SRC)" python3 forge/tests/test_switch_law.py

plugin: panel-check
	cmake -S plugin -B build/plugin -G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
		-DFETCHCONTENT_SOURCE_DIR_JUCE="$(JUCE_SRC)"
	cmake --build build/plugin --target Shogun_VST3 ShogunProbe -j 8
	./build/plugin/ShogunProbe_artefacts/Release/ShogunProbe build/plugin/tabs
	codesign --force --sign - --timestamp=none \
		build/plugin/Shogun_artefacts/Release/VST3/SHOGUN.vst3

# Linux path (the box): JUCE 8.0.4 from a local checkout, no network fetch. VST3 + the probe (8 tab PNGs + checks).
JUCE_LINUX ?= /workspace/JUCE
plugin-linux:
	cmake -S plugin -B build/plugin-linux -G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DFETCHCONTENT_SOURCE_DIR_JUCE="$(JUCE_LINUX)"
	cmake --build build/plugin-linux --target Shogun_VST3 ShogunProbe -j 8

probe-linux: panel-check plugin-linux
	./build/plugin-linux/ShogunProbe_artefacts/Release/ShogunProbe build/plugin-linux/tabs

install-vst: plugin
	mkdir -p "$(HOME)/Library/Audio/Plug-Ins/VST3"
	ln -sfn "$(CURDIR)/build/plugin/Shogun_artefacts/Release/VST3/SHOGUN.vst3" \
		"$(HOME)/Library/Audio/Plug-Ins/VST3/SHOGUN.vst3"

ENGINE_DEPS = engine/shogun.cpp $(wildcard engine/*.h engine/*.inc engine/voices/*.h) \
	$(wildcard $(JIDAI_COMMON)/jidai/*.h $(JIDAI_COMMON)/jidai/jcs/*.h $(JIDAI_COMMON)/jidai/dsp/*.h)
TEST_SRCS = tests/main.cpp tests/blocks.cpp tests/voices.cpp tests/engine.cpp tests/mod.cpp tests/factory.cpp
TEST_DEPS = $(TEST_SRCS) tests/testutil.h tests/rig.h tests/data/levelcomp_cases.h plugin/Source/PanelLayout.inc

build/shogun_tests: $(ENGINE_DEPS) $(TEST_DEPS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(JIDAI_INC) -Itests -o $@ engine/shogun.cpp $(TEST_SRCS)

asan: $(ENGINE_DEPS) $(TEST_DEPS)
	mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -Iengine $(JIDAI_INC) -Itests \
		-o build/shogun_tests_asan engine/shogun.cpp $(TEST_SRCS)
	./build/shogun_tests_asan

# The factory bank (engine/factory_bank.inc) from scripts/make_factory.py: builds the documents, trims each kit's
# levels to its peak target with tools/factory_fmt (native renders at 44.1 and 48 kHz) and writes the canonical text.
build/factory_fmt: $(ENGINE_DEPS) tools/factory_fmt.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(JIDAI_INC) -o $@ tools/factory_fmt.cpp engine/shogun.cpp

factory: build/factory_fmt
	python3 scripts/make_factory.py

# Calibration probe (§9.5, §15.5 step 1): prints calib = target peak / measured noon peak per voice.
# `make calib` rewrites engine/calib_table.inc from a calib = 1 build.
build/measure_calib: $(ENGINE_DEPS) tools/measure_calib.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(JIDAI_INC) -o $@ tools/measure_calib.cpp engine/shogun.cpp

calib: build/measure_calib
	./build/measure_calib table > engine/calib_table.inc.new && mv engine/calib_table.inc.new engine/calib_table.inc
	./build/measure_calib

# The web page: the engine compiled to wasm (clang with the wasm32 target and wasm-ld, no libc),
# checked sample by sample against the native build, then inlined into web/shogun.html.
WASM_FLAGS = --target=wasm32 -std=c++17 -O2 -nostdlib -nostdinc -isystem web/wasm/include -Iengine $(JIDAI_INC) -DSHOGUN_NO_FORMAT \
	-fno-exceptions -fno-rtti -fno-builtin -Wall -Wextra -Werror \
	-Wl,--no-entry -Wl,--allow-undefined -Wl,-z,stack-size=262144

build/shogun.wasm: $(ENGINE_DEPS) web/wasm/shogun_web.cpp web/wasm/shogun_web.h $(wildcard web/wasm/include/*)
	mkdir -p build
	clang++ $(WASM_FLAGS) -o $@ engine/shogun.cpp web/wasm/shogun_web.cpp

build/web_parity: $(ENGINE_DEPS) web/wasm/shogun_web.cpp web/wasm/shogun_web.h tests/web_parity.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(JIDAI_INC) -o $@ engine/shogun.cpp web/wasm/shogun_web.cpp tests/web_parity.cpp

web: build/shogun.wasm build/web_parity
	node web/test_wasm.mjs
	node web/build_page.mjs
	node web/test_page.mjs

# Re-measure the LEVEL table in web/page/kits.js after a kit or engine change, then make web.
web-levels: build/shogun.wasm
	node web/make_levels.mjs

# JIDAI RACK strict build: the rack compiles SHOGUN's engine, the vendored headers and the plugin sources with JUCE's
# recommended warning flags (juce_recommended_warning_flags, JUCEHelperTargets.cmake) plus the strict set below, as
# errors. `make strict` checks the engine, the tests, the web facade and every plugin/Source file with both clang++
# and g++. Only JUCE's own headers are -isystem; the vendored jidai-common headers are checked like our own.
STRICT_SET = -Wfloat-equal -Wshadow -Wconversion -Wdouble-promotion -Werror
JUCE_WARN_CLANG = -Wall -Wextra -Wshadow-all -Wshorten-64-to-32 -Wstrict-aliasing -Wuninitialized -Wunused-parameter \
	-Wconversion -Wsign-compare -Wint-conversion -Wconditional-uninitialized -Wconstant-conversion -Wsign-conversion \
	-Wbool-conversion -Wextra-semi -Wunreachable-code -Wcast-align -Wshift-sign-overflow -Wmissing-prototypes \
	-Wnullable-to-nonnull-conversion -Wno-ignored-qualifiers -Wswitch-enum -Wpedantic -Wdeprecated -Wfloat-equal \
	-Wmissing-field-initializers -Wzero-as-null-pointer-constant -Wunused-private-field -Woverloaded-virtual -Wreorder \
	-Winconsistent-missing-destructor-override
JUCE_WARN_GCC = -Wall -Wextra -Wpedantic -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wsign-compare \
	-Wsign-conversion -Wunreachable-code -Wcast-align -Wno-implicit-fallthrough -Wno-maybe-uninitialized \
	-Wno-ignored-qualifiers -Wno-multichar -Wswitch-enum -Wredundant-decls -Wno-strict-overflow -Wshadow -Wfloat-equal \
	-Wmissing-field-initializers -Woverloaded-virtual -Wreorder -Wzero-as-null-pointer-constant
STRICT_CLANG = $(JUCE_WARN_CLANG) $(STRICT_SET) -Wimplicit-int-float-conversion
STRICT_GCC = $(JUCE_WARN_GCC) $(STRICT_SET)
STRICT_JUCE = -isystem $(JUCE_LINUX)/modules -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 \
	-DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 -DJUCE_VST3_CAN_REPLACE_VST2=0 -DNDEBUG=1 \
	$(foreach m,audio_basics audio_devices audio_formats audio_processors audio_utils core data_structures events graphics gui_basics gui_extra,-DJUCE_MODULE_AVAILABLE_juce_$(m)=1)
STRICT_SRCS = engine/shogun.cpp $(TEST_SRCS) web/wasm/shogun_web.cpp tests/web_parity.cpp tools/measure_calib.cpp tools/factory_fmt.cpp
PLUGIN_SRCS = plugin/Source/PluginProcessor.cpp plugin/Source/PluginEditor.cpp plugin/Source/Probe.cpp

strict: strict-clang strict-gcc

strict-clang:
	@for f in $(STRICT_SRCS); do echo "clang++ strict $$f"; clang++ -std=c++17 -O2 $(STRICT_CLANG) -Iengine $(JIDAI_INC) -Itests -fsyntax-only $$f || exit 1; done
	@for f in $(PLUGIN_SRCS); do echo "clang++ strict $$f"; clang++ -std=c++17 -O2 $(STRICT_CLANG) -Iengine $(JIDAI_INC) -Iplugin/Source $(STRICT_JUCE) -fsyntax-only $$f || exit 1; done

strict-gcc:
	@for f in $(STRICT_SRCS); do echo "g++ strict $$f"; g++ -std=c++17 -O2 $(STRICT_GCC) -Iengine $(JIDAI_INC) -Itests -fsyntax-only $$f || exit 1; done
	@for f in $(PLUGIN_SRCS); do echo "g++ strict $$f"; g++ -std=c++17 -O2 $(STRICT_GCC) -Iengine $(JIDAI_INC) -Iplugin/Source $(STRICT_JUCE) -fsyntax-only $$f || exit 1; done

clean:
	rm -rf build
