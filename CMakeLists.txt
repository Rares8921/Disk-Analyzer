cmake_minimum_required(VERSION 3.11)
project(DiskAnalyzer C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

SET(GCC_COVERAGE_COMPILE_FLAGS "-DCLION -DDEBUG")
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

find_library(CRYPTO_LIB crypto REQUIRED)
if (NOT CRYPTO_LIB)
    message(FATAL_ERROR "Could not find OpenSSL's crypto library.")
endif()

add_executable(daemon Daemon/Daemon.c Daemon/TaskManager.c Daemon/TaskManager.h Daemon/EventManager.c Daemon/EventManager.h Daemon/ShmManager.c Daemon/ShmManager.h Daemon/Utility.h Daemon/Utility.c Daemon/Analyzer.h Daemon/Analyzer.c)
target_link_libraries(daemon ${CRYPTO_LIB})

add_executable(da da.c)
target_link_libraries(da ${CRYPTO_LIB})
