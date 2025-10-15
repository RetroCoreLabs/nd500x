cmake_minimum_required(VERSION 3.16)
add_executable(nd500wasm main.c)
target_include_directories(nd500wasm PRIVATE ${CMAKE_SOURCE_DIR}/src/cpu ${CMAKE_SOURCE_DIR}/src/machine ${CMAKE_SOURCE_DIR}/src/debugger)
target_link_libraries(nd500wasm PRIVATE nd500_machine nd500_cpu nd500_ndlib nd500_debugger cjson)

# Copy web files to build output at build time (not configure time)
add_custom_command(TARGET nd500wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/index.html
        ${CMAKE_BINARY_DIR}/bin/index.html
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/style.css
        ${CMAKE_BINARY_DIR}/bin/style.css
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/debugger.js
        ${CMAKE_BINARY_DIR}/bin/debugger.js
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/frontend/nd500wasm/web/demo/kernel
        ${CMAKE_BINARY_DIR}/bin/kernel
    COMMENT "Copying web files to build output"
)

