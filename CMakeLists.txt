cmake_minimum_required(VERSION 3.10)

project(c-vulkan)

set(SOURCES src/vulkan.c src/main.c)

find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE src ${Vulkan_INCLUDE_DIRS})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
