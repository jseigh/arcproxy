cmake_minimum_required(VERSION 3.20)

project(arcproxy)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -std=gnu17 -ggdb")

#set(ADDITIONAL_CLEAN_FILES include lib)

add_library(arcproxy STATIC
    src/arcproxy.c
)

target_include_directories(arcproxy PUBLIC
    src
)

set_property(TARGET arcproxy
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES include lib
)

#${CMAKE_INSTALL_INCLUDEDIR}
install(FILES src/arcproxy.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
#${CMAKE_INSTALL_LIBDIR}
install(TARGETS arcproxy DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)

#target_compile_options(arcproxy PUBLIC ()


