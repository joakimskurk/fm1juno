cmake_minimum_required(VERSION 3.16)
project(fm1juno LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fm1juno_core STATIC
    src/control106/dac106.c
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

add_executable(fm1juno_tests tests/unit/test_main.c)
target_link_libraries(fm1juno_tests PRIVATE fm1juno_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fm1juno_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME fm1juno_unit COMMAND fm1juno_tests)
