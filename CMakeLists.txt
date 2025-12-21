cmake_minimum_required(VERSION 3.15)
project(bayan VERSION 1.0.0 LANGUAGES CXX)

# Настройка политик CMake
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0092 NEW)
cmake_policy(SET CMP0167 NEW)

# Настройка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройка путей вывода

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Настройки для MSYS2/MinGW
if(WIN32)
    if(EXISTS "C:/msys64/mingw64")
        set(MSYS2_PREFIX "C:/msys64/mingw64")
        list(APPEND CMAKE_PREFIX_PATH ${MSYS2_PREFIX})
        list(APPEND CMAKE_LIBRARY_PATH "${MSYS2_PREFIX}/lib")
        list(APPEND CMAKE_INCLUDE_PATH "${MSYS2_PREFIX}/include")
        message(STATUS "MSYS2 environment detected: ${MSYS2_PREFIX}")
    endif()
    
    if(MSVC)
        add_compile_options(/W4 /WX /permissive- /MP)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    
    if(MINGW)
        add_compile_options(-Wall -Wextra -Werror -pedantic -Wa,-mbig-obj)
        add_link_options(-static -static-libgcc -static-libstdc++)
    endif()
endif()

# Для Linux/macOS
if(UNIX AND NOT APPLE)
    add_compile_options(-Wall -Wextra -Werror -pedantic)
elseif(APPLE)
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Настройки поиска Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Компоненты Boost
set(BOOST_COMPONENTS filesystem program_options)

# Поиск Boost
find_package(Boost 1.65.0 REQUIRED COMPONENTS ${BOOST_COMPONENTS})

if(Boost_FOUND)
    message(STATUS "=========================================")
    message(STATUS "Boost found: ${Boost_VERSION}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "=========================================")
else()
    if(WIN32)
        message(FATAL_ERROR "Boost not found! Install with: pacman -S mingw-w64-x86_64-boost")
    else()
        message(FATAL_ERROR "Boost not found! Install development packages")
    endif()
endif()

# Включение директорий
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})

# Список исходных файлов
set(SOURCE_FILES
    main.cpp
    ScanDir.cpp
    Hash.cpp
)

# Создание исполняемого файла
add_executable(bayan ${SOURCE_FILES})

# Подключение Boost библиотек
target_link_libraries(bayan PRIVATE
    Boost::filesystem
    Boost::program_options
)

# Дополнительные библиотеки для Windows
if(WIN32)
    target_link_libraries(bayan PRIVATE ws2_32)
endif()

# Опции компиляции
target_compile_options(bayan PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:Debug>:-g -O0>
)

# Установка
install(TARGETS bayan
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)