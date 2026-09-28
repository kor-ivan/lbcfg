# GenerateVersion.cmake

# Принимаем аргументы, переданные из CMake
set(CURRENT_SOURCE_DIR "${CMAKE_ARGV3}")
set(CURRENT_BINARY_DIR "${CMAKE_ARGV4}")
set(PROJECT_VERSION    "${CMAKE_ARGV5}")
set(BUILD_TYPE         "${CMAKE_ARGV6}") # Сюда прилетит Debug, Release и т.д.

# Если тип сборки не задан (например, в некоторых Multi-configuration генераторах), пишем по умолчанию
if(NOT BUILD_TYPE)
    set(BUILD_TYPE "Default")
endif()

find_package(Git QUIET)

set(GIT_COMMIT_HASH "n/a")
set(GIT_DIRTY_STATUS "")

if(GIT_FOUND AND EXISTS "${CURRENT_SOURCE_DIR}/.git")
    # 1. Получаем короткий хеш
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # 2. Проверяем наличие изменений (dirty)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT "${GIT_STATUS_OUTPUT}" STREQUAL "")
        set(GIT_DIRTY_STATUS "-dirty")
    endif()
endif()

if(NOT "${GIT_COMMIT_HASH}" STREQUAL "n/a")
    set(FULL_VERSION_STRING "${PROJECT_VERSION} ${BUILD_TYPE} ${GIT_COMMIT_HASH}${GIT_DIRTY_STATUS}")
else()
    set(FULL_VERSION_STRING "${PROJECT_VERSION} ${BUILD_TYPE}")
endif()

# Генерируем временный заголовочный файл
configure_file(
    "${CURRENT_SOURCE_DIR}/version.h.in"
    "${CURRENT_BINARY_DIR}/version.h.tmp"
    @ONLY
)

# Перезаписываем version.h только при реальных изменениях (защита от лишней перекомпиляции)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CURRENT_BINARY_DIR}/version.h.tmp"
    "${CURRENT_BINARY_DIR}/version.h"
)
