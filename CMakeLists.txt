cmake_minimum_required(VERSION 3.20)

project(cad_test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ...
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER clang++)

# Автоматическая обработка Qt (сигналы/слоты, .ui, ресурсы)
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_AUTORCC ON)

# Поиск зависимостей
# find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED)


file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})


target_link_libraries(${PROJECT_NAME} PRIVATE
  # Qt5::Core 
  # Qt5::Widgets
  Qt5::Widgets 
  Qt5::OpenGL
)

# Флаги оптимизации для Clang
target_compile_options(${PROJECT_NAME} PRIVATE -O2 -Wall -Wextra)

# Для корректной работы с X11 в WSLg/WSL2 можно явно указать вывод
if(DEFINED ENV{DISPLAY})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_X11")
endif()



# cmake_minimum_required(VERSION 3.10)
# project(STLViewer LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_COMPILER clang++)

# # Поиск компонентов Qt5
# find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED)

# add_executable(stl_viewer main.cpp)

# # Линковка библиотек Qt
# target_link_libraries(stl_viewer PRIVATE Qt5::Widgets Qt5::OpenGL)

# # Флаги оптимизации для Clang
# target_compile_options(stl_viewer PRIVATE -O2 -Wall -Wextra)

# # Для корректной работы с X11 в WSLg/WSL2 можно явно указать вывод
# if(DEFINED ENV{DISPLAY})
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_X11")
# endif()