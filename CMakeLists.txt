cmake_minimum_required(VERSION 3.15)
project(ConnectSphereSocialNetwork VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(server
    src/main.cpp
)

if(WIN32)
    target_link_libraries(server PRIVATE sqlite3 ws2_32 crypt32)
else()
    find_package(SQLite3 REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(server PRIVATE SQLite::SQLite3 Threads::Threads)
endif()
