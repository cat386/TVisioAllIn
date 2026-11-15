# Параметры сборки: TV_BUILD_EXAMPLES (по умолчанию ON) — собирать
# примеры приложений. TV_BUILD_USING_GPM (по умолчанию ON, только Linux)
# — поддерживать мышь в консоли Linux через сервер GPM; отдельная
# библиотека не требуется. TV_USE_STATIC_RTL (по умолчанию OFF) —
# связывать со статической библиотекой времени выполнения, только MSVC.
# TV_REDUCE_APP_SIZE (по умолчанию OFF) — уменьшать размер исполняемых
# файлов, удаляя неиспользуемые функции и данные при компоновке; влияет
# на PUBLIC-флаги компоновки библиотеки. TV_OPTIMIZE_BUILD (по умолчанию
# ON) — использовать предкомпилированные заголовки, требуется CMake 3.16
# или новее. TV_LIBRARY_UNITY_BUILD (по умолчанию OFF) — ускорять сборку
# основной библиотеки за счёт объединения единиц трансляции; требуется
# CMake 3.16 или новее, размер библиотеки может увеличиться.
# TV_BUILD_TESTS (по умолчанию OFF) — собирать тесты; запуск через CTest.
#
#

cmake_minimum_required (VERSION 3.5...3.30)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.13.0")
    cmake_policy(SET CMP0077 NEW) # 'option()' учитывает обычные переменные.
endif()

set(MASTER_PROJECT FALSE)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MASTER_PROJECT TRUE)
endif()

function(tv_message mode)
    if (MASTER_PROJECT)
        set(msg)
    else()
        set(msg "(${PROJECT_NAME}) ")
    endif()
    foreach(i ${ARGN})
        set(msg "${msg}${i}")
    endforeach()
    message(${mode} ${msg})
endfunction()

function(tv_message_mp)
    if (MASTER_PROJECT)
        tv_message(${ARGN})
    endif()
endfunction()

project(tvision)

# Параметры проекта

include(CheckCXXSourceCompiles)

check_cxx_source_compiles("
    #define _doDeclare(a, b) int a ## b;
    #define _expand(a, ...) _doDeclare(a, __VA_ARGS__)
    #define _declare(a) _expand(a, __COUNTER__)

    _declare(a)
    _declare(a)
    _declare(a)

    int main() { return a1; }
"   SUPPORTS_COUNTER_MACRO)

function(tv_enable_unity target)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16.0" AND SUPPORTS_COUNTER_MACRO)
        set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
    endif()
endfunction()

set(TV_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(tv_set_output_dir target)
    # Для удобства размещаем все результаты в каталоге сборки.
    set_target_properties(${target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${TV_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${TV_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${TV_BINARY_DIR}
    )
endfunction()

function(tv_add_private_includes target)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/include/tvision"
        "${PROJECT_SOURCE_DIR}/include/tvision/compat/borland"
    )
    if (NOT WIN32)
        target_include_directories(${target} PRIVATE
            "${PROJECT_SOURCE_DIR}/include/tvision/compat/windows"
        )
    endif()
    if (NOT WIN32 AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android"))
        target_include_directories(${target} PRIVATE
            "${PROJECT_SOURCE_DIR}/include/tvision/compat/malloc"
        )
    endif()
endfunction()

function(tv_set_warnings target)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE
            /wd4068 # Неизвестные директивы pragma.
            /wd4146 # Беззнаковый результат, несмотря на знак минус.
            /wd4166 # Недопустимое соглашение о вызовах конструктора или деструктора.
            /wd4244 # Возможная потеря данных при преобразовании типа.
            /wd4250 # Наследование с доминированием.
            /wd4267 # Возможная потеря данных при преобразовании size_t в другой тип.
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wno-deprecated
            -Wno-unknown-pragmas
            -Wno-pragmas
            -Wno-missing-field-initializers
        )
    endif()
endfunction()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(TV_BUILD_USING_GPM "Use GPM" ON)
    set(MAY_BUILD_USING_GPM TRUE)
endif()

if (MASTER_PROJECT)
    option(TV_BUILD_EXAMPLES "Build example apps" ON)
endif()

option(TV_USE_STATIC_RTL "Link against the static version of the runtime library (MSVC only)" OFF)
if (MSVC)
    set(MAY_USE_STATIC_RTL TRUE)
elseif (TV_USE_STATIC_RTL)
    tv_message(WARNING "'TV_USE_STATIC_RTL' requested but only available in MSVC or equivalent.")
    set(TV_USE_STATIC_RTL OFF)
endif()

option(TV_REDUCE_APP_SIZE "Strip out unused functions and data from executables" OFF)
if ((CMAKE_VERSION VERSION_LESS "3.13.0") AND TV_REDUCE_APP_SIZE)
    tv_message(WARNING "'TV_REDUCE_APP_SIZE' requested but only available in CMake 3.13.0 or newer.")
    set(TV_REDUCE_APP_SIZE OFF)
endif()

option(TV_LIBRARY_UNITY_BUILD "Use CMake's Unity Build" OFF)
if ((CMAKE_VERSION VERSION_LESS "3.16.0") AND TV_LIBRARY_UNITY_BUILD)
    tv_message(WARNING "'TV_LIBRARY_UNITY_BUILD' requested but only available in CMake 3.16.0 or newer.")
    set(TV_LIBRARY_UNITY_BUILD OFF)
endif()

option(TV_OPTIMIZE_BUILD "Use CMake's Precompiled Headers" ON)


include(GNUInstallDirs)

tv_message_mp(STATUS "Install path: ${CMAKE_INSTALL_PREFIX}")
tv_message(STATUS "Build Examples: ${TV_BUILD_EXAMPLES}")
if (MAY_BUILD_USING_GPM)
    tv_message(STATUS "Build w/GPM: ${TV_BUILD_USING_GPM}")
endif()
if (MAY_USE_STATIC_RTL)
    tv_message_mp(STATUS "Link w/static RTL: ${TV_USE_STATIC_RTL}")
endif()

option(TV_BUILD_TESTS "Build tests (run with CTest)" OFF)
if (TV_BUILD_TESTS)
    enable_testing()
endif()

# Библиотека

add_subdirectory(source)

# Примеры

add_subdirectory(examples)

# Тесты

add_subdirectory(test)

# Инструменты разработки требуют современного компилятора C++17.
option(TV_BUILD_TOOLS "Build TV Studio C++ developer tools" ${MASTER_PROJECT})
if (TV_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
