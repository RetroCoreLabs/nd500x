cmake_minimum_required(VERSION 3.16)
add_library(nd500_ndlib ndlib.c ndlib_aout.c ndlib_symbols.c ndlib_color.c ndlib_dom.c ndlib_dom_loader.c
                        nd_tty.c nd_compat.c)
target_include_directories(nd500_ndlib PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${ND500X_ROOT}/src/machine ${ND500X_ROOT}/src/cpu ${ND500X_ROOT}/include)

if(DEFINED HAVE_LIBSYMBOLS)
    target_include_directories(nd500_ndlib PUBLIC ${ND500X_ROOT}/external/libsymbols/include)
    if(TARGET symbols_objects)
        target_link_libraries(nd500_ndlib PUBLIC symbols_objects)
    endif()
endif()

