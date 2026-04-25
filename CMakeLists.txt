cmake_minimum_required(VERSION 4.1)
project(liked_song_memories C)

set(CMAKE_C_STANDARD 17)
set(CURL_ROOT /opt/homebrew/opt/curl)

find_package(CURL REQUIRED)
find_package(notcurses REQUIRED)
include_directories(/opt/homebrew/opt/notcurses/include)
link_directories(/opt/homebrew/opt/notcurses/lib)

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
add_executable(tui_example tui_example.c)
add_executable(main_tui main_tui_integration.c main_functions.c)
add_executable(main_tui_table main_tui_table.c main_functions.c)
add_executable(main_tui_clean main_tui_clean.c main_functions.c)
add_executable(main_tui_inline_art main_tui_inline_art.c main_functions.c)
target_include_directories(main_tui_clean PRIVATE ${cjson_SOURCE_DIR})
target_include_directories(main_tui_clean PRIVATE ${CURL_INCLUDE_DIRS})
target_include_directories(main_tui_clean PRIVATE /opt/homebrew/opt/notcurses/include)
target_include_directories(main_tui_inline_art PRIVATE ${cjson_SOURCE_DIR})
target_include_directories(main_tui_inline_art PRIVATE ${CURL_INCLUDE_DIRS})
target_include_directories(main_tui_inline_art PRIVATE /opt/homebrew/opt/notcurses/include)
target_link_libraries(main_tui_clean PRIVATE ${CURL_LIBRARIES} cjson notcurses-core notcurses notcurses-ffi)
target_link_libraries(main_tui_inline_art PRIVATE ${CURL_LIBRARIES} cjson notcurses-core notcurses notcurses-ffi)

target_include_directories(liked_song_memories PRIVATE ${cjson_SOURCE_DIR})
target_include_directories(liked_song_memories PRIVATE ${CURL_INCLUDE_DIRS})
target_include_directories(liked_song_memories PRIVATE ${NOTCURSES_INCLUDE_DIRS})
target_link_libraries(liked_song_memories PRIVATE ${CURL_LIBRARIES} cjson ${NOTCURSES_LIBRARIES})
