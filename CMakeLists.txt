cmake_minimum_required(VERSION 4.1)
project(liked_song_memories C)
set(CMAKE_C_STANDARD 17)
add_definitions(-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600)
find_package(CURL REQUIRED)
find_package(Notcurses REQUIRED)
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
add_executable(main_tui main_tui.c main_functions.c)
target_include_directories(liked_song_memories PRIVATE ${cjson_SOURCE_DIR} ${CURL_INCLUDE_DIRS} ${NOTCURSES_INCLUDE_DIRS})
target_link_libraries(liked_song_memories PRIVATE ${CURL_LIBRARIES} cjson ${NOTCURSES_LIBRARIES})
target_include_directories(main_tui PRIVATE ${cjson_SOURCE_DIR} ${CURL_INCLUDE_DIRS} ${NOTCURSES_INCLUDE_DIRS})
target_link_libraries(main_tui PRIVATE ${CURL_LIBRARIES} cjson notcurses notcurses-core)
