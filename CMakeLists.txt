cmake_minimum_required(VERSION 3.25)
set(VOCALCHAIN_VERSION "0.1.0" CACHE STRING "VocalChain release version")
project(VocalChain VERSION ${VOCALCHAIN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VOCALCHAIN_BUILD_TESTS "Build VocalChain tests" ON)
set(VOCALCHAIN_JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout")

if(VOCALCHAIN_JUCE_SOURCE_DIR)
    add_subdirectory("${VOCALCHAIN_JUCE_SOURCE_DIR}" JUCE)
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
add_subdirectory(apps/desktop)

if(VOCALCHAIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
