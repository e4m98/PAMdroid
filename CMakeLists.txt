cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0074 NEW)

set(CMAKE_SYSTEM_PROCESSOR x86_64)

project(App VERSION 1.0.0
        DESCRIPTION "bluetooth_Linux_auth")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/sdbus-c++")
set(C_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Building for 64-bit architecture")
else()
    message(FATAL_ERROR "This project requires 64-bit architecture")
endif()

if (ARMA_64BIT_WORD)
  add_definitions(-DARMA_64BIT_WORD)
endif()

# ------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDBUS REQUIRED IMPORTED_TARGET sdbus-c++)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
find_path(PAM_INCLUDE_DIR NAMES security/pam_appl.h)
find_library(PAM_LIBRARY NAMES pam)

# ------------------------------------------------------------------
file(GLOB_RECURSE SOURCES src/*.c)
file(GLOB_RECURSE HEADERS include/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# ------------------------------------------------------------------
target_include_directories(${PROJECT_NAME}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${PAM_INCLUDE_DIR}
)

target_compile_options(${PROJECT_NAME}
  PRIVATE
    -Wall -Wextra -Wconversion
    -m64
    $<$<CONFIG:Debug>:-g3;-Og;-DDEBUG>
    $<$<CONFIG:Release>:-O3;-DNDEBUG;-march=x86-64>
)

target_link_options(${PROJECT_NAME}
  PRIVATE
    -m64
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    _FILE_OFFSET_BITS=64
    _LARGEFILE64_SOURCE
)

# ------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    PkgConfig::SDBUS
    PkgConfig::GLIB2
    ${PAM_LIBRARY}
)

# ------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

# ------------------------------------------------------------------
message(STATUS "=== Build Configuration ===")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Language: C")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Pointer Size: ${CMAKE_SIZEOF_VOID_P} bytes")
message(STATUS "C Install Prefix: ${C_INSTALL_PREFIX}")
message(STATUS "SDBUS Found: ${SDBUS_FOUND}")
message(STATUS "GLIB2 Found: ${GLIB2_FOUND}")
message(STATUS "PAM Library: ${PAM_LIBRARY}")
message(STATUS "Source files: ${SOURCES}")
message(STATUS "===========================")
