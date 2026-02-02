
.PHONY: build configure test clang-tidy clean
.PHONY: build-debug build-release build-asan build-ubsan
.PHONY: test-debug test-release test-asan test-ubsan

# Default preset (debug)
PRESET ?= debug

# Main targets using default or specified preset
build:
	cmake --build --preset "$(PRESET)"

configure:
	cmake --preset "$(PRESET)"

test: build
	ctest --preset "$(PRESET)" --output-on-failure --verbose

# Specific build targets
build-debug:
	cmake --build --preset "debug"

build-release:
	cmake --build --preset "release"

build-asan:
	cmake --build --preset "asan"

build-ubsan:
	cmake --build --preset "ubsan"

# Specific test targets
test-debug: build-debug
	ctest --preset "debug" --output-on-failure --verbose

test-release: build-release
	ctest --preset "release" --output-on-failure --verbose

test-asan: build-asan
	ctest --preset "asan" --output-on-failure --verbose

test-ubsan: build-ubsan
	ctest --preset "ubsan" --output-on-failure --verbose

# Static analysis
clang-tidy:
	find include/ \( -iname "*.h" -or -iname "*.hpp" -or -iname "*.cpp" \) -print0 | parallel -0 clang-tidy -p build/$(PRESET) {}

# Cleanup
clean:
	rm -rf build/
