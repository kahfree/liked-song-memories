cmake_minimum_required(VERSION 4.1)
project(liked_song_memories C)

set(CMAKE_C_STANDARD 17)

set(CURL_ROOT /opt/homebrew/opt/curl)

find_package(CURL REQUIRED)

add_executable(liked_song_memories main.c)

target_include_directories(liked_song_memories PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(liked_song_memories PRIVATE ${CURL_LIBRARIES})