BUILD_TESTS ?= false
BUILD_LUA_BINDINGS ?= false
BUILD_SAMPLES ?= false
BUILD_TYPE ?= "Debug"

IMVUE_USE_LUAJIT ?= false

JINJA := $(wildcard tools/templates/*.j2)

.PHONY: test test-leaks test-common samples check

# builds everything
all: BUILD_SAMPLES=true
all: BUILD_LUA_BINDINGS=true
all: BUILD_TESTS=true
all: build

bindings: $(JINJA)
	make -C tools bindings

build: bindings
	@mkdir -p build
	cd build && cmake ../ -DIMVUE_USE_LUAJIT=$(IMVUE_USE_LUAJIT) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DBUILD_TESTS=$(BUILD_TESTS) -DBUILD_LUA_BINDINGS=$(BUILD_LUA_BINDINGS) -DBUILD_SAMPLES=$(BUILD_SAMPLES) && cmake --build .

test-common: BUILD_TESTS=true
test-common: BUILD_LUA_BINDINGS=true

test-common: build

test: test-common
	cd ./build/tests && ./unit-tests

benchmark: BUILD_TYPE=Release

benchmark: test-common
	cd ./build/tests && ./benchmark

test-leaks: test-common
	echo "Running unit tests with Valgrind memory leaks check enabled"
	cd ./build/tests && valgrind --error-exitcode=42 --leak-check=full ./unit-tests

# build samples along with lua bindings
samples: BUILD_LUA_BINDINGS=true
samples: BUILD_SAMPLES=true
samples: build

check:
	cppcheck --max-configs=1 --suppressions-list=.cppcheck -I src/ -I imgui/ src/ tests/ samples/simple/ --error-exitcode=7

ci: bindings check test-leaks benchmark
