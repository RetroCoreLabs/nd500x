# Simple CMake wrapper Makefile

.PHONY: all clean run wasm wasm-clean wasm-serve
.PHONY: with-dap without-dap with-sanitizer without-sanitizer
.PHONY: dap-sanitizer help

BUILD_DIR?=build
WASM_DIR?=build_wasm

# Default build (native with DAP if available)
all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

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
	@rm -rf $(BUILD_DIR) $(WASM_DIR)

wasm:
	@mkdir -p $(WASM_DIR)
	@cd $(WASM_DIR) && emcmake cmake -DBUILD_WASM=ON .. && $(MAKE)

wasm-clean:
	@rm -rf $(WASM_DIR)

wasm-serve: wasm
	@echo "Starting web server on http://localhost:8000"
	@echo "Open http://localhost:8000 in your browser to use the ND500X Web Debugger"
	@cd $(WASM_DIR)/bin && python3 -m http.server 8000

help:
	@echo "ND500X Build Targets:"
	@echo "  make                 - Build native (with DAP if available)"
	@echo "  make with-dap        - Build with DAP support (requires external/libdap)"
	@echo "  make without-dap     - Build without DAP support"
	@echo "  make with-sanitizer  - Build with address sanitizer"
	@echo "  make dap-sanitizer   - Build with DAP + sanitizer"
	@echo "  make wasm            - Build WebAssembly version"
	@echo "  make wasm-serve      - Build WASM and start web server"
	@echo "  make run             - Build and run in debug mode"
	@echo "  make clean           - Clean all build directories"
	@echo ""
	@echo "Build directories:"
	@echo "  build/               - Native build output"
	@echo "  build_wasm/          - WASM build output"


