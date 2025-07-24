cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0074 NEW)

project(App VERSION 1.0.0
        DESCRIPTION "bluetooth_Linux_auth_bypass"
        LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 사용자 설치 경로
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/cmake/sdbus-c++")
set(CPP_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
option(ARMA_64BIT_WORD "Use 64-bit Armadillo indices" ON)

if (ARMA_64BIT_WORD)
  add_definitions(-DARMA_64BIT_WORD)
endif()


# ------------------------------------------------------------------
# 필수 의존성 (Config 모드 우선)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS REQUIRED sdbus-c++)
pkg_check_modules(GLIB2 REQUIRED
                  IMPORTED_TARGET
                  glib-2.0)
# ------------------------------------------------------------------
# 헤더 온리 라이브러리 include 경로 등록 예시

# HTSlib
# find_path(HTSLIB_INCLUDE_DIR htslib/hts.h
#   PATHS "${CPP_INSTALL_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
# find_library(HTSLIB_LIBRARY hts
#   PATHS "${CPP_INSTALL_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
# add_library(HTSlib::hts STATIC IMPORTED GLOBAL)
# set_target_properties(HTSlib::hts PROPERTIES
#   IMPORTED_LOCATION "${HTSLIB_LIBRARY}"
#   INTERFACE_INCLUDE_DIRECTORIES "${HTSLIB_INCLUDE_DIR}"
# )

# ------------------------------------------------------------------
# 프로젝트 타겟
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.hpp include/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${SDBUS_INCLUDE_DIRS}"
)

# 컴파일 옵션
target_compile_options(${PROJECT_NAME}
  PRIVATE
    -Wall -Wextra -Wconversion
    $<$<CONFIG:Debug>:-g3;-Og;-DDEBUG>
    $<$<CONFIG:Release>:-O3;-DNDEBUG;-march=native>
)

# ------------------------------------------------------------------
# 라이브러리 링크
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    ${SDBUS_LIBRARIES}
    PkgConfig::GLIB2
)
# ------------------------------------------------------------------
# 5) 설치 규칙
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# ------------------------------------------------------------------
# 빌드 요약
message(STATUS "=== Build Configuration ===")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CPP Install Prefix: ${CPP_INSTALL_PREFIX}")
message(STATUS "Source files: ${SOURCES}")
message(STATUS "===========================")
