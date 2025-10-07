cmake_minimum_required(VERSION 3.10)

project(seftool VERSION 1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Sources
add_executable(seftool
    src/action.c
    src/babe.c
    src/break.c
    src/certz.c
    src/common.c
    src/cmd.c
    src/connection.c
    src/csloader.c
    src/flash.c
    src/gdfs.c
    src/loader.c
    src/main.c
    src/scandir.c
    src/serial.c
    src/sha1.c
    src/vkp.c
    src/zipper.c
)

# --- Set static libs ---
set(BUILD_SHARED_LIBS OFF)
set(ENABLE_COMMONCRYPTO OFF)
set(ZLIB_USE_STATIC_LIBS ON)

# --- Helper to find static libraries explicitly ---
function(find_static_library LIB_NAME OUT_VAR)
    if (WIN32 OR MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif()

    find_library(${OUT_VAR} NAMES ${LIB_NAME})

    if (NOT ${OUT_VAR})
        message(FATAL_ERROR "Static library '${LIB_NAME}' not found.")
    else()
        message(STATUS "Found static library: ${${OUT_VAR}}")
    endif()
endfunction()

# --- Dependencies (prefer static) ---
# libserialport
find_static_library(serialport SERIALPORT_LIB)

if (WIN32)
    target_link_libraries(seftool PRIVATE
        ${SERIALPORT_LIB}
        setupapi
        cfgmgr32
        advapi32
    )
else()
    target_link_libraries(seftool PRIVATE ${SERIALPORT_LIB})
endif()

# --- Zlib ---
find_static_library(z ZLIB_LIB)
if (NOT ZLIB_LIB)
    message(FATAL_ERROR "Zlib static library not found")
endif()

# --- BZip2 (optional, but required by Minizip on MSYS2) ---
find_static_library(bz2 BZIP2_LIB)

# --- Minizip ---
find_static_library(minizip MINIZIP_LIB)
if (NOT MINIZIP_LIB)
    message(FATAL_ERROR "Minizip static library not found")
endif()

target_link_libraries(seftool PRIVATE
    ${MINIZIP_LIB}
    ${ZLIB_LIB}
    ${BZIP2_LIB}
)

# --- Compiler / linker flags ---
if (MSVC)
    target_compile_definitions(seftool PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    if (BUILD_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # careful: full -static on Linux often breaks
        target_link_options(seftool PRIVATE -static -static-libgcc)
    elseif (MINGW)
        # Windows portable EXE
        target_link_options(seftool PRIVATE -static -static-libgcc)
    endif()

    if (CMAKE_BUILD_TYPE MATCHES "Release")
        target_link_options(seftool PRIVATE -s)
    endif()

    target_compile_options(seftool PRIVATE -Wall -Wextra -O2 -Wno-missing-braces)
endif()

set(RESOURCE_DIRS loader rest break49 anycid)
foreach(DIR ${RESOURCE_DIRS})
    if (WIN32)
        add_custom_command(TARGET seftool POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/${DIR}
                ${CMAKE_BINARY_DIR}/${DIR})
    else()
        add_custom_command(TARGET seftool POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                ${CMAKE_SOURCE_DIR}/${DIR}
                ${CMAKE_BINARY_DIR}/${DIR})
    endif()
endforeach()