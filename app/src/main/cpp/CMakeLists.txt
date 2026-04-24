cmake_minimum_required(VERSION 3.22.1)

project(parcial)

# Ahora sí busca OpenCV
find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_library(parcial SHARED native-lib.cpp)

target_link_libraries(parcial
        ${OpenCV_LIBS}
        log
)