cmake_minimum_required(VERSION 3.16)
project(signal-parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

add_executable(signal-parser
    src/main.cpp
    src/SignalParser.cpp
    src/ZmqBridge.cpp
)

target_include_directories(signal-parser PRIVATE
    src
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(signal-parser PRIVATE
    ${ZMQ_LIBRARIES}
    pthread
)

install(TARGETS signal-parser DESTINATION bin)
