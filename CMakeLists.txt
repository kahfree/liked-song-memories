cmake_minimum_required(VERSION 4.1)
project(liked_song_memories C)

set(CMAKE_C_STANDARD 17)
set(CURL_ROOT /opt/homebrew/opt/curl)

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
        cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
        GIT_TAG        v1.7.18
)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(cjson)

add_executable(liked_song_memories main.c)

target_include_directories(liked_song_memories PRIVATE ${cjson_SOURCE_DIR})
target_include_directories(liked_song_memories PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(liked_song_memories PRIVATE ${CURL_LIBRARIES} cjson)
