cmake_minimum_required(VERSION 3.16)

project(lbcfg VERSION 0.1.0 LANGUAGES CXX)
option(ENABLE_AUTO_DEPLOY "Enable automated windeployqt packaging after build" OFF)
option(ENABLE_INSTALLER_BUILD "Run binarycreator to build the installer .exe after deploy" OFF)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
# Проверяем тип сборки для Single-config генераторов (Ninja, Make)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_LBMBC_EXE OFF)
    set(ALLOWED_LBMBC_CONFIG "None") # Для Debug запрещаем установку exe
else()
    set(BUILD_LBMBC_EXE ON)

    # Запоминаем имя конфигурации. Если оно не задано (по умолчанию), пишем Release
    if(NOT CMAKE_BUILD_TYPE)
        set(ALLOWED_LBMBC_CONFIG "Release")
    else()
        set(ALLOWED_LBMBC_CONFIG ${CMAKE_BUILD_TYPE})
    endif()
endif()

# Передаем управление в поддиректорию lbmbc
add_subdirectory(lbmbc)

set(EXE_SOURCES main.cpp)

set(LIB_SOURCES
        mainwindow.cpp
        mainwindow.h
        GenerateVersion.cmake
        appsettings.h appsettings.cpp
        discoverdockwidget.h discoverdockwidget.cpp
        devicetreedockwidget.h devicetreedockwidget.cpp
        configdockwidget.h configdockwidget.cpp
        plcmanager.h plcmanager.cpp
        firmwarewidget.h firmwarewidget.cpp
        firmwarerepositorydialog.h firmwarerepositorydialog.cpp
        firmwareanalyzer.h firmwareanalyzer.cpp
        commandmanager.h commandmanager.cpp
        mainmenu.h mainmenu.cpp
        logmanager.h logmanager.cpp
        logdockwidget.h logdockwidget.cpp
        yamltextview.h yamltextview.cpp
        varview.h varview.cpp
        deviceview.h deviceview.cpp
        watchdockwidget.h watchdockwidget.cpp
        watchsession.h watchsession.cpp
        resources.qrc
        version.h.in
        lbcfg_global.h
)

# === АВТО-ВЕРСИЯ: Создаем таргет для ежесборочной проверки Git и типа сборки ===
add_custom_target(lbcfg_version_check ALL
    COMMAND ${CMAKE_COMMAND}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_VERSION}"
        "${CMAKE_BUILD_TYPE}" # Передаем тип сборки шестым аргументом (ARGV6)
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    COMMENT "Checking Git repository status and build type for version.h..."
)

add_library(lbcfg_lib SHARED ${LIB_SOURCES})

# Передаем макрос компилятора, сигнализирующий о сборке библиотеки
target_compile_definitions(lbcfg_lib PRIVATE MY_SHARED_LIB_LIBRARY)

set_target_properties(lbcfg_lib PROPERTIES
    OUTPUT_NAME "lbcfg"   # Имя файла будет lbcfg.dll
    PREFIX "lib"          # Явно добавляем lib в начало (будет liblbcfg.dll)
)

# Линкуем библиотеку с Qt и внутренней поддиректорией
target_link_libraries(lbcfg_lib PRIVATE Qt${QT_VERSION_MAJOR}::Widgets lbmbc_lib)
target_include_directories(lbcfg_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Привязываем генерацию version.h к библиотеке
add_dependencies(lbcfg_lib lbcfg_version_check)

# === 3. СОЗДАЕМ ИСПОЛНЯЕМЫЙ ФАЙЛ (EXE) ===
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(lbcfg
        MANUAL_FINALIZATION
        ${EXE_SOURCES}
        app_resources.rc
        DeployWindows.cmake
    )
else()
    if(ANDROID)
        add_library(lbcfg SHARED
            ${EXE_SOURCES}
        )
    else()
        add_executable(lbcfg
            ${EXE_SOURCES}
            app_resources.rc
        )
    endif()
endif()

target_link_libraries(lbcfg PRIVATE lbcfg_lib Qt${QT_VERSION_MAJOR}::Widgets)
target_include_directories(lbcfg PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


# === АВТО-ВЕРСИЯ: Подключаем сгенерированные заголовочные файлы к lbcfg ===
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.lbcfg)
endif()
set_target_properties(lbcfg PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# === КРОСС ПЛАТФОРМЕННЫЙ ДЕПЛОЙМЕНТ ===
if(ENABLE_AUTO_DEPLOY)
    if(WIN32)
        # Подключаем изолированный сценарий деплоя для Windows
        include(DeployWindows.cmake)
    elseif(LINUX)
        # Сюда в будущем добавится include(DeployLinux.cmake) для вызова linuxdeployqt / cpack
        message(STATUS "Linux deployment script will be integrated here.")
    endif()
endif()

# Обычное копирование бинарников для локального запуска в корне билда (Оставляем только для Windows Release)
if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET lbcfg POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:lbmbc_lib>" "$<TARGET_FILE_DIR:lbcfg>"
    )
    if(TARGET lbmbc)
        add_custom_command(TARGET lbcfg POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:lbmbc>" "$<TARGET_FILE_DIR:lbcfg>"
        )
    endif()
endif()
