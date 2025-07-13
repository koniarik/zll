
.PHONY: build configure test clang-tidy

build:
	cmake --build --preset "default"

configure:
	cmake --preset "default" $(if $(SANITIZER), -DCMAKE_CXX_FLAGS="-fsanitize=$(SANITIZER)")

test: build
	ctest --preset "default" --output-on-failure --verbose

clang-tidy:
	find include/ \( -iname "*.h" -or -iname "*.cpp" \) -print0 | parallel -0 clang-tidy -p build/default {}
