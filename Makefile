# Simple CMake wrapper Makefile

.PHONY: all clean run wasm wasm-clean wasm-serve

BUILD_DIR?=build
WASM_DIR?=build_wasm

all:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

run: all
	@./$(BUILD_DIR)/bin/nd500x --debug

clean:
	@rm -rf $(BUILD_DIR)

wasm:
	@mkdir -p $(WASM_DIR)
	@cd $(WASM_DIR) && emcmake cmake -DBUILD_WASM=ON .. && $(MAKE)

wasm-clean:
	@rm -rf $(WASM_DIR)

wasm-serve: wasm
	@echo "Starting web server on http://localhost:8000"
	@echo "Open http://localhost:8000 in your browser to use the ND500X Web Debugger"
	@cd $(WASM_DIR)/bin && python3 -m http.server 8000


