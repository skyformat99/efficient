cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(MODEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        CACHE PATH "Path to Efficient include files.")

add_subdirectory(src)
