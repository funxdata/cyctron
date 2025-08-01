cmake_minimum_required(VERSION 3.30)
project(cyctron C)

# 设置 C 标准，C11 是常用标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 把所有源文件放进一个变量
set(SOURCES
    main.c
    server.c
    task_queue.c
)

add_executable(cyctron_server ${SOURCES})

# 链接 pthread 库
find_package(Threads REQUIRED)
target_link_libraries(cyctron_server PRIVATE Threads::Threads)