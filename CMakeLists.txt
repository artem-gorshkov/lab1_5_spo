cmake_minimum_required(VERSION 3.19)
project(lab1_5_spo C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c storage.c storage.h json_api.c json_api.h xml_api.h)

if (APPLE)
include_directories(/usr/local/Cellar/json-c/0.15/include)
target_link_libraries(server /usr/local/Cellar/json-c/0.15/lib/libjson-c.dylib)
else()
target_link_libraries(server json-c)
endif()


add_executable(client client.c storage.h json_api.c json_api.h
        ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h xml_api.h)

target_include_directories(client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (APPLE)
find_package(FLEX  REQUIRED)
set(BISON_EXECUTABLE /usr/local/opt/bison/bin/bison)
find_package(BISON 3.0.0 REQUIRED)
add_definitions(-DPLATFORM_MACOS)
target_link_libraries(client /usr/local/Cellar/json-c/0.15/lib/libjson-c.dylib)
else()
    find_package(BISON 3.0.0 REQUIRED)
    find_package(FLEX  REQUIRED)
    target_link_libraries(client json-c)
endif ()

add_custom_command(
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/ql.lex"
        PRE_BUILD
        COMMAND ${FLEX_EXECUTABLE}
        ARGS -o"${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c" "${CMAKE_CURRENT_SOURCE_DIR}/ql.lex"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c"
)

add_custom_command(
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/ql.y"
        PRE_BUILD
        COMMAND ${BISON_EXECUTABLE}
        ARGS -y -d --debug --verbose -o"${CMAKE_CURRENT_BINARY_DIR}/y.tab.c" -Wcounterexamples
        "${CMAKE_CURRENT_SOURCE_DIR}/ql.y"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/y.tab.c"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/y.tab.h"
)
