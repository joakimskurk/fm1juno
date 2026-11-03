cmake_minimum_required(VERSION 3.16)
project(fm1juno LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fm1juno_core STATIC
    src/common/event_queue.c
    src/control106/assigner106.c
    src/control106/dac106.c
    src/control106/control_math106.c
    src/control106/envelope106.c
    src/control106/event_replay106.c
    src/control106/lfo106.c
    src/control106/lfo_delay106.c
    src/control106/module_control106.c
    src/control106/portamento106.c
    src/control106/pitch106.c
    src/control106/control_scheduler106.c
    src/timer106/timer8253_106.c
)

target_include_directories(fm1juno_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fm1juno_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(fm1juno_ref reference/main.cpp)
target_link_libraries(fm1juno_ref PRIVATE fm1juno_core)

add_executable(fm1juno_render reference/render_main.cpp)
target_link_libraries(fm1juno_render PRIVATE fm1juno_core)

add_executable(fm1juno_tests tests/unit/test_main.c)
target_link_libraries(fm1juno_tests PRIVATE fm1juno_core)

add_executable(fm1juno_module_tests tests/unit/test_module_control.c)
target_link_libraries(fm1juno_module_tests PRIVATE fm1juno_core)

add_executable(fm1juno_replay_tests tests/unit/test_event_replay.c)
target_link_libraries(fm1juno_replay_tests PRIVATE fm1juno_core)

add_executable(fm1juno_event_queue_tests tests/unit/test_event_queue.c)
target_link_libraries(fm1juno_event_queue_tests PRIVATE fm1juno_core)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fm1juno_ref PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(fm1juno_render PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(fm1juno_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(fm1juno_module_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(fm1juno_replay_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(fm1juno_event_queue_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME fm1juno_unit COMMAND fm1juno_tests)
add_test(NAME fm1juno_module_unit COMMAND fm1juno_module_tests)
add_test(NAME fm1juno_replay_unit COMMAND fm1juno_replay_tests)
add_test(NAME fm1juno_event_queue_unit COMMAND fm1juno_event_queue_tests)
add_test(
    NAME fm1juno_render_smoke
    COMMAND fm1juno_render
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/p1_callback_invariance_v1.csv
            --outer-frames 128
            --samples 256
)
