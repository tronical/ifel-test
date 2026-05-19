cmake_minimum_required(VERSION 3.21)
project(ifel-test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Slint 1.10 REQUIRED)


add_executable(ifel-test src/main.cpp)
target_link_libraries(ifel-test PRIVATE Slint::Slint)
target_compile_definitions(ifel-test PRIVATE
    SLINT_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ui/main.slint")
