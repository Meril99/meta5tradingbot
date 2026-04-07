cmake_minimum_required(VERSION 3.16)
project(telegram-mt5-bot VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find common dependencies ─────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)

# ── Sub-projects ─────────────────────────────────────────────────────────────
# tdlib-listener and signal-parser replaced by Python telegram-parser
add_subdirectory(services/mt5-bridge)
