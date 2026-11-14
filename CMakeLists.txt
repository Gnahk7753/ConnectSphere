cmake_minimum_required(VERSION 3.15)
project(ConnectSphereSocialNetwork VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(server
    src/main.cpp
)

target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(server PRIVATE SQLite3::SQLite3 Threads::Threads nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32 crypt32 advapi32)
endif()
