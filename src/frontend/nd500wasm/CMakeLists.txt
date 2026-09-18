cmake_minimum_required(VERSION 3.16)
add_executable(nd500wasm main.c)
target_link_libraries(nd500wasm PRIVATE nd500x_warnings)   # house C standard, Tier A
target_include_directories(nd500wasm PRIVATE ${ND500X_ROOT}/src/cpu ${ND500X_ROOT}/src/machine ${ND500X_ROOT}/src/debugger)
target_link_libraries(nd500wasm PRIVATE nd500_machine nd500_cpu nd500_ndlib nd500_debugger cjson)

# Copy web files to build output at build time (not configure time)
add_custom_command(TARGET nd500wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/src/frontend/nd500wasm/web/index.html
        ${CMAKE_BINARY_DIR}/bin/index.html
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/src/frontend/nd500wasm/web/style.css
        ${CMAKE_BINARY_DIR}/bin/style.css
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/src/frontend/nd500wasm/web/debugger.js
        ${CMAKE_BINARY_DIR}/bin/debugger.js
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ND500X_ROOT}/examples/05-c-kernel/kernel
        ${CMAKE_BINARY_DIR}/bin/kernel
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/examples/05-c-kernel/kernel.zip
        ${CMAKE_BINARY_DIR}/bin/kernel.zip
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/src/frontend/nd500wasm/web/test_source_loading.html
        ${CMAKE_BINARY_DIR}/bin/test_source_loading.html
    COMMAND ${CMAKE_COMMAND} -E copy
        ${ND500X_ROOT}/src/frontend/nd500wasm/web/test_zip_loading.html
        ${CMAKE_BINARY_DIR}/bin/test_zip_loading.html
    COMMENT "Copying web files to build output"
)

