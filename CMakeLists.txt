cmake_minimum_required(VERSION 3.30)

project(bd C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES src/main.c src/hangar.c src/main.c src/players.c src/utils.c
            src/ui.c src/matches.c)

set(HEADERS include/hangar.h include/players.h include/utils.h include/ui.h include/matches.h luigi/luigi.h /usr/include/freetype2/freetype.h)

add_executable(${PROJECT_NAME} ${SOURCES})
find_package(PostgreSQL REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE include luigi /usr/include/freetype2)
target_link_libraries(${PROJECT_NAME} PRIVATE PostgreSQL::PostgreSQL m X11 -lfreetype)
add_compile_definitions(UI_FREETYPE)
