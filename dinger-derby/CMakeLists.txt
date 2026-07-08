cmake_minimum_required(VERSION 3.10)
project(dinger-derby)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

enable_testing()

set(ENGINE_SOURCES
    src/math/Vector2.cpp
    src/math/Vector3.cpp
    src/math/Matrix4.cpp
    src/physics/Body2D.cpp
    src/physics/PhysicsWorld2D.cpp
    src/collision/Collision2D.cpp
)

add_library(dinger_engine STATIC
    ${ENGINE_SOURCES}
)

target_include_directories(dinger_engine PUBLIC src)

function(add_dinger_demo target source)
    add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE
            dinger_engine
            SFML::Graphics
            SFML::Window
            SFML::System
    )
endfunction()

add_dinger_demo(user_ball_test tests/user_ball_test.cpp)
add_dinger_demo(random_spawn_test tests/random_spawn_test.cpp)
add_dinger_demo(bat_test tests/bat_test.cpp)
add_dinger_demo(spin_test tests/spin_test.cpp)

add_executable(physics_engine_test tests/physics_engine_test.cpp)
target_link_libraries(physics_engine_test PRIVATE dinger_engine)
add_test(NAME physics_engine_test COMMAND physics_engine_test)

add_executable(math3d_test tests/math3d_test.cpp)
target_link_libraries(math3d_test PRIVATE dinger_engine)
add_test(NAME math3d_test COMMAND math3d_test)
