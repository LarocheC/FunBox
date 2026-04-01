cmake_minimum_required(VERSION 3.22)
project(FunBoxPlugins VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JUCE - user must have JUCE installed or provide JUCE_DIR
# Option 1: Use CPM to fetch JUCE automatically
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Add plugin subdirectories
add_subdirectory(Venus)
