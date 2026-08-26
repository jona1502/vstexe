cmake_minimum_required(VERSION 3.25)
set(INPUTRACK_VERSION "0.1.0" CACHE STRING "InputRack release version")
project(InputRack VERSION ${INPUTRACK_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INPUTRACK_BUILD_TESTS "Build InputRack tests" ON)
set(INPUTRACK_JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(INPUTRACK_JUCE_SOURCE_DIR)
    add_subdirectory("${INPUTRACK_JUCE_SOURCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

add_subdirectory(platform)
add_subdirectory(engine)
add_subdirectory(update)
add_subdirectory(apps/scanner)
add_subdirectory(apps/desktop)

if(INPUTRACK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
