cmake_minimum_required(VERSION 3.16)
project(ParametricSketcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим Qt5
find_package(Qt5 COMPONENTS Core Widgets OpenGL REQUIRED)

# Находим OpenGL
find_package(OpenGL REQUIRED)

# Создаем исполняемый файл
add_executable(cad_test
    src/main.cpp
    # src/solid_model/solid_model.cpp
    src/stl_model/stl_model.cpp
)

# Подключаем Qt и OpenGL
target_link_libraries(cad_test PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::OpenGL
    ${OPENGL_LIBRARIES}
)

# Определяем макросы
target_compile_definitions(cad_test PRIVATE 
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# Включаем заголовочные файлы
target_include_directories(cad_test PRIVATE
    ${OPENGL_INCLUDE_DIR}
    src
)