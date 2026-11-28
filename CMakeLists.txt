cmake_minimum_required(VERSION 3.20)
project(AshenCrownRPG LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- 1. raylib (5.5) ---
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

# --- 2. nlohmann/json (3.11.3, header-only) ---
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib nlohmann_json)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/GameManager.cpp
    src/managers/ResourceManager.cpp
    src/managers/DataManager.cpp
    src/managers/MapManager.cpp
    src/managers/EntityManager.cpp
    src/managers/CombatManager.cpp
    src/managers/SkillManager.cpp
    src/managers/QuestManager.cpp
    src/managers/EconomyManager.cpp
    src/managers/InputManager.cpp
    src/managers/SaveManager.cpp
    src/managers/UIManager.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /W3)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# Copy data and assets alongside the built executable for standalone launching
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Syncing data/ and assets/ to executable directory"
)
