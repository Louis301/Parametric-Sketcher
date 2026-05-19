cmake_minimum_required(VERSION 3.20)

project(cad_test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Автоматическая обработка Qt (сигналы/слоты, .ui, ресурсы)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Поиск зависимостей
find_package(Qt5 REQUIRED COMPONENTS Core Sql Widgets)
find_package(PostgreSQL REQUIRED)

# Сборка исполняемого файла
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

# Линковка
target_link_libraries(${PROJECT_NAME} PRIVATE
  Qt5::Core 
  Qt5::Sql 
  Qt5::Widgets
  PostgreSQL::PostgreSQL
)
