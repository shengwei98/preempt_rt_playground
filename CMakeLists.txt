project(preempt_rt_playground)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(runner 
main.cpp)

target_link_libraries(runner 
Threads::Threads)


