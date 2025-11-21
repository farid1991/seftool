cmake_minimum_required(VERSION 3.10)

project(seftool VERSION 1.05 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Source files
file(GLOB_RECURSE SEFTOOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Executable
add_executable(seftool ${SEFTOOL_SOURCES})

# Include directories
target_include_directories(seftool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/emp/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eric/common
)

# --- Set static libs ---
set(BUILD_SHARED_LIBS OFF)
set(ENABLE_COMMONCRYPTO OFF)
set(ZLIB_USE_STATIC_LIBS ON)

if (WIN32)
    # --- Helper to find static libraries (Windows only) ---
    function(find_static_library LIB_NAME OUT_VAR)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")
        find_library(${OUT_VAR} NAMES ${LIB_NAME})
        if (NOT ${OUT_VAR})
            message(FATAL_ERROR "Static library '${LIB_NAME}' not found.")
        else()
            message(STATUS "Found static library: ${${OUT_VAR}}")
        endif()
    endfunction()

    # --- Dependencies for Windows ---
    find_static_library(serialport SERIALPORT_LIB)
    find_static_library(z ZLIB_LIB)
    find_static_library(bz2 BZIP2_LIB)
    find_static_library(minizip MINIZIP_LIB)

    target_link_libraries(seftool PRIVATE
        ${SERIALPORT_LIB}
        ${MINIZIP_LIB}
        ${ZLIB_LIB}
        ${BZIP2_LIB}
        setupapi
        cfgmgr32
        advapi32
    )

else()
    # --- Dependencies for Linux / macOS ---

    # libserialport
    find_path(SERIALPORT_INCLUDE_DIR libserialport.h)
    find_library(SERIALPORT_LIB serialport)
    if (NOT SERIALPORT_LIB)
        message(FATAL_ERROR "libserialport not found.")
    endif()

    # zlib
    find_package(ZLIB REQUIRED)

    # bzip2 (optional)
    find_package(BZip2)
    if (BZIP2_FOUND)
        target_link_libraries(seftool PRIVATE BZip2::BZip2)
    endif()

    # minizip
    find_path(MINIZIP_INCLUDE_DIR minizip/zip.h)
    find_library(MINIZIP_LIB minizip)
    if (NOT MINIZIP_LIB)
        message(FATAL_ERROR "minizip not found.")
    endif()

    # --- Link all ---
    target_include_directories(seftool PRIVATE
        ${SERIALPORT_INCLUDE_DIR}
        ${MINIZIP_INCLUDE_DIR}
    )
    target_link_libraries(seftool PRIVATE
        ${SERIALPORT_LIB}
        ${MINIZIP_LIB}
        ZLIB::ZLIB
    )
endif()

# --- libusb & libftdi
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(libusb REQUIRED IMPORTED_TARGET libusb-1.0)
# target_link_libraries(seftool PRIVATE PkgConfig::libusb)
# pkg_check_modules(libftdi1 REQUIRED IMPORTED_TARGET libftdi1)
# find_package(libftdi1 REQUIRED)
# target_link_libraries(seftool PRIVATE PkgConfig::libftdi1)

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
