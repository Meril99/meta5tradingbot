cmake_minimum_required(VERSION 3.16)
project(telegram-mt5-bot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find common dependencies ─────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# TDLib — installed to /usr/local by install_deps.sh
find_package(Td QUIET)

# ── Sub-projects ─────────────────────────────────────────────────────────────
add_subdirectory(services/tdlib-listener)
add_subdirectory(services/signal-parser)
add_subdirectory(services/mt5-bridge)
