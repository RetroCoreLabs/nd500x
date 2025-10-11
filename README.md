ND500X Emulator (scaffold)

Build
- Native: mkdir -p build && cd build && cmake .. && make
- Run: ./bin/nd500x --debug
- WASM (Emscripten): mkdir -p build_wasm && cd build_wasm && emcmake cmake -DBUILD_WASM=ON .. && make

Dependencies
- Native: libcjson (pkg-config), optional: external/libdap, external/libsymbols
- WASM: cJSON fetched automatically via FetchContent

Layout
- src/ndlib: logging
- src/machine: byte-addressed memory, bus, unified debugger API
- src/cpu: CPU state and step skeleton
- src/debugger: CLI that calls machine APIs (m, d, step, regs, load, run, stop)
- src/frontend/nd500x: native entry
- src/frontend/nd500wasm: wasm entry and JSON debug exports


