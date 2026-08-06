# Simple CMake wrapper Makefile

.PHONY: all clean run wasm wasm-clean wasm-serve kernel-example
.PHONY: with-dap without-dap with-sanitizer without-sanitizer
.PHONY: dap-sanitizer release help

BUILD_DIR?=build
BUILD_DIR_RELEASE?=build_release
WASM_DIR?=build_wasm

# CMake generator selection.
# On Windows (w64devkit / MSYS2) Ninja has to be forced: left alone, CMake
# picks the Visual Studio generator, which needs MSVC and cannot drive a MinGW
# toolchain. On Linux and macOS the default (Unix Makefiles) is right.
ifeq ($(OS),Windows_NT)
    CMAKE_GENERATOR_FLAG := -G Ninja
else
    CMAKE_GENERATOR_FLAG :=
endif

# Parallel job count. nproc is Linux; macOS spells it sysctl -n hw.ncpu.
JOBS?=$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Default build (native with DAP if available)
all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

# Optimised build, output in build_release/. This is what CI packages, on every
# platform - keep it working on Linux, macOS and Windows alike.
release:
	@mkdir -p $(BUILD_DIR_RELEASE)
	cmake -S . -B $(BUILD_DIR_RELEASE) $(CMAKE_GENERATOR_FLAG) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR_RELEASE) -j $(JOBS)
	@echo ""
	@echo "Release build complete: $(BUILD_DIR_RELEASE)/bin/"

# Build without DAP support (removes external/libdap from build)
without-dap:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DSKIP_LIBDAP=ON .. && $(MAKE)

# Build with DAP support explicitly (requires external/libdap)
with-dap:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -d external/libdap ]; then \
		echo "Error: external/libdap not found. Run: git submodule update --init"; \
		exit 1; \
	fi
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

# Build with address sanitizer
with-sanitizer:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" .. && $(MAKE)

# Build without sanitizer (default)
without-sanitizer: all

# Build with both DAP and sanitizer
dap-sanitizer:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -d external/libdap ]; then \
		echo "Error: external/libdap not found. Run: git submodule update --init"; \
		exit 1; \
	fi
	@cd $(BUILD_DIR) && cmake -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" .. && $(MAKE)

run: all
	@./$(BUILD_DIR)/bin/nd500x --debug

clean:
	@rm -rf $(BUILD_DIR) $(BUILD_DIR_RELEASE) $(WASM_DIR)

wasm:
	@mkdir -p $(WASM_DIR)
	@cd $(WASM_DIR) && emcmake cmake -DBUILD_WASM=ON .. && $(MAKE)

wasm-clean:
	@rm -rf $(WASM_DIR)

# Build the C kernel example (compile, assemble, link, package)
kernel-example:
	@echo "Building C kernel example..."
	@$(MAKE) -C examples/05-c-kernel all
	@echo "✓ Kernel example built: examples/05-c-kernel/kernel.zip"

wasm-serve: wasm kernel-example
	@echo "Copying kernel.zip to WASM build directory..."
	@cp examples/05-c-kernel/kernel.zip $(WASM_DIR)/bin/kernel.zip
	@echo "Starting web server on http://localhost:8000"
	@echo "Open http://localhost:8000 in your browser to use the ND500X Web Debugger"
	@cd $(WASM_DIR)/bin && python3 -m http.server 8000

help:
	@echo "ND500X Build Targets:"
	@echo "  make                 - Build native (with DAP if available)"
	@echo "  make release         - Optimised build into build_release/ (what CI ships)"
	@echo "  make with-dap        - Build with DAP support (requires external/libdap)"
	@echo "  make without-dap     - Build without DAP support"
	@echo "  make with-sanitizer  - Build with address sanitizer"
	@echo "  make dap-sanitizer   - Build with DAP + sanitizer"
	@echo "  make wasm            - Build WebAssembly version"
	@echo "  make kernel-example  - Build C kernel example (compile, assemble, link, zip)"
	@echo "  make wasm-serve      - Build WASM, kernel example, and start web server"
	@echo "  make run             - Build and run in debug mode"
	@echo "  make clean           - Clean all build directories"
	@echo ""
	@echo "Windows note: build.bat wraps 'make release' with w64devkit on PATH."
	@echo ""
	@echo "Build directories:"
	@echo "  build/               - Native build output"
	@echo "  build_release/       - Optimised build output"
	@echo "  build_wasm/          - WASM build output"
	@echo ""
	@echo "Kernel example workflow:"
	@echo "  1. make kernel-example        - Builds kernel.zip in examples/05-c-kernel/"
	@echo "  2. make wasm-serve            - Builds kernel.zip, WASM, copies to build_wasm/bin/, starts server"
	@echo "  3. Open http://localhost:8000 - Load kernel.zip in web debugger"


