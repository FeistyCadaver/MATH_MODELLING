cmake_minimum_required(VERSION 3.16)

project(lab1var3)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(MotionGL)

add_executable(lab1var3
    main.cpp
    numerical_methods.cpp
)

target_link_libraries(lab1var3 PRIVATE motiongl)