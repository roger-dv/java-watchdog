cmake_minimum_required(VERSION 3.19)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

project(watchdog)

if(DEFINED ENV{OS} AND "$ENV{OS}" STREQUAL "Windows_NT")
    set(OS_PLATFORM "win32")
else()
    set(OS_PLATFORM "linux")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-unknown-pragmas -std=c11")

set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -std=gnu++20 -static-libstdc++ -static-libgcc")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'$ORIGIN/'")

set(SOURCE_FILES main.cpp format2str.cpp format2str.h log.cpp log.h decl-exception.cpp decl-exception.h ini.cpp ini.h
    cfgparse.cpp cfgparse.h path-concat.cpp path-concat.h)

SET(LIBRARY_OUTPUT_PATH "${watchdog_SOURCE_DIR}/${CMAKE_BUILD_TYPE}")

SET(EXECUTABLE_OUTPUT_PATH "${LIBRARY_OUTPUT_PATH}")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} popt)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
)