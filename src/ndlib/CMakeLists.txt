cmake_minimum_required(VERSION 3.16)
add_library(nd500_ndlib ndlib.c ndlib_aout.c ndlib_symbols.c ndlib_color.c)
target_include_directories(nd500_ndlib PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_SOURCE_DIR}/src/machine)

if(DEFINED HAVE_LIBSYMBOLS)
    target_include_directories(nd500_ndlib PUBLIC ${CMAKE_SOURCE_DIR}/external/libsymbols/include)
    if(TARGET symbols_objects)
        target_link_libraries(nd500_ndlib PUBLIC symbols_objects)
    endif()
endif()

