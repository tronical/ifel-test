cmake_minimum_required(VERSION 3.21)
project(ifel-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Slint 1.10 QUIET)
if(NOT Slint_FOUND)
    message(STATUS "Slint not found via find_package — building from source")

    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG release/1.10
        SOURCE_SUBDIR api/cpp
    )

    set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(Slint)
else()
    message(STATUS "Using installed Slint ${Slint_VERSION}")
endif()

add_executable(ifel-test src/main.cpp)
target_link_libraries(ifel-test PRIVATE Slint::Slint)
target_compile_definitions(ifel-test PRIVATE
    SLINT_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ui/main.slint")
