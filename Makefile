
.PHONY: build configure test clang-tidy clean
.PHONY: build-debug build-release build-asan build-ubsan
.PHONY: test-debug test-release test-asan test-ubsan
.PHONY: test-pprinter

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

# Pretty-printer tests (runs inside Alpine Docker with GDB)
GDB_IMAGE ?= zll-gdb

test-pprinter:
	docker build -f docker/Dockerfile.gdb -t $(GDB_IMAGE) .
	docker run --rm -v "$(CURDIR):/src" $(GDB_IMAGE) sh -c \
		"cmake -B /build -S /src -G Ninja -DZLL_TESTS_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug && \
		 cmake --build /build && \
		 ctest --test-dir /build -R 'gdb_test' --output-on-failure --verbose"

# Cleanup
clean:
	rm -rf build/
