cmake_minimum_required(VERSION 3.10)
project(dinger-derby)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

set(ENGINE_SOURCES
    src/math/Vector2.cpp
    src/physics/Body2D.cpp
    src/physics/PhysicsWorld2D.cpp
    src/collision/Collision2D.cpp
)

add_executable(user_ball_test
    tests/user_ball_test.cpp
    ${ENGINE_SOURCES}
)

add_executable(random_spawn_test
    tests/random_spawn_test.cpp
    ${ENGINE_SOURCES}
)

add_executable(bat_test
    tests/bat_test.cpp
    ${ENGINE_SOURCES}
)

add_executable(spin_test
    tests/spin_test.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(user_ball_test PRIVATE src)
target_include_directories(random_spawn_test PRIVATE src)
target_include_directories(bat_test PRIVATE src)
target_include_directories(spin_test PRIVATE src)
target_link_libraries(user_ball_test
    SFML::Graphics
    SFML::Window
    SFML::System
)

target_link_libraries(random_spawn_test
    SFML::Graphics
    SFML::Window
    SFML::System
)

target_link_libraries(bat_test
    SFML::Graphics
    SFML::Window
    SFML::System
)

target_link_libraries(spin_test
    SFML::Graphics
    SFML::Window
    SFML::System
)