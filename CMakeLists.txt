cmake_minimum_required(VERSION 3.10)

project(seftool VERSION 1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Sources
file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.c)
add_executable(seftool ${SRC_FILES})

# --- Set static libs ---  (for future update)
# set(ZLIB_USE_STATIC_LIBS TRUE)

# --- Dependencies ---
# libserialport
find_library(SERIALPORT_LIB NAMES serialport libserialport REQUIRED)
if (WIN32)
    target_link_libraries(seftool PRIVATE ${SERIALPORT_LIB} setupapi cfgmgr32 advapi32)
else()
    target_link_libraries(seftool PRIVATE ${SERIALPORT_LIB})
endif()

# zlib (for future update)
# find_package(ZLIB REQUIRED)
# target_link_libraries(seftool PRIVATE ZLIB::ZLIB)

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

if(WIN32)
    add_custom_command(TARGET seftool POST_BUILD
        # Copy source directories into the build tree
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/loader
            ${CMAKE_BINARY_DIR}/loader

        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/rest
            ${CMAKE_BINARY_DIR}/rest
    )
else()
    add_custom_command(TARGET seftool POST_BUILD
        # Create symlinks from build tree to source directories
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/loader
            ${CMAKE_BINARY_DIR}/loader

        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/rest
            ${CMAKE_BINARY_DIR}/rest
    )
endif()
