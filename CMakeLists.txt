cmake_minimum_required(VERSION 3.30)

project(bd C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_DIR src)
set(HEADER_DIR include)

include_directories(${HEADER_DIR})

file(GLOB SOURCES "${SOURCE_DIR}/*.c")

add_executable(${PROJECT_NAME} ${SOURCES})
find_package(PostgreSQL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PostgreSQL::PostgreSQL)
