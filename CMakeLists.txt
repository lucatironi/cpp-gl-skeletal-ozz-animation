cmake_minimum_required(VERSION 3.16)

# Project Setup
project(cpp-gl-skeletal-ozz-animation LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include Dependencies
add_subdirectory(deps)

set(SOURCE_FILES
    src/main.cpp
)

set(HEADER_FILES
    inc/animated_model.hpp
    inc/mesh.hpp
    inc/model_loader.hpp
    inc/shader.hpp
    inc/texture_2d.hpp
)

# Add Executable First
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

# Include Directories
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/inc
)

# Copy Shaders to Output Directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

# Copy Assets to Output Directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Link External Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad glm assimp stb ozz_animation_offline ozz_animation)