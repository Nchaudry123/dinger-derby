cmake_minimum_required(VERSION 3.10)
project(dinger-derby)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(dinger-derby
    src/main.cpp
    src/math/Vector2.cpp
    src/physics/Body2D.cpp
    src/physics/PhysicsWorld2D.cpp
    src/collision/Collision2D.cpp
)

target_link_libraries(dinger-derby
    SFML::Graphics
    SFML::Window
    SFML::System
)