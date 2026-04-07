cmake_minimum_required(VERSION 3.16)
project(tdlib-listener LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Td REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

add_executable(tdlib-listener
    src/main.cpp
    src/TelegramClient.cpp
    src/ZmqPublisher.cpp
)

target_include_directories(tdlib-listener PRIVATE
    src
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(tdlib-listener PRIVATE
    Td::TdStatic
    ${ZMQ_LIBRARIES}
    pthread
)

install(TARGETS tdlib-listener DESTINATION bin)
