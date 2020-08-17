cmake_minimum_required(VERSION 3.10)

project(dir2src)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES_CXX
    "src/main.cpp"
)

set(DIRECTORY_PACKER_INCLUDE_DIRS
    "src"
)

add_executable(dir2src ${SOURCES_CXX})
target_include_directories(dir2src PRIVATE ${INCLUDE_DIRS})
target_link_libraries(dir2src Kernel32)
