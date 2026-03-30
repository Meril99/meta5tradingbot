cmake_minimum_required(VERSION 3.16)
project(mt5-bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(CURL REQUIRED)

add_executable(mt5-bridge
    src/main.cpp
    src/OrderReceiver.cpp
    src/TradeEvent.cpp
    src/TradeEventReceiver.cpp
    src/TelegramNotifier.cpp
    src/TradeJournal.cpp
)

target_include_directories(mt5-bridge PRIVATE
    src
    ${ZMQ_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(mt5-bridge PRIVATE
    ${ZMQ_LIBRARIES}
    ${CURL_LIBRARIES}
    pthread
)

install(TARGETS mt5-bridge DESTINATION bin)
