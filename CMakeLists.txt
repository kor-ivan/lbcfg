cmake_minimum_required(VERSION 3.16)

project(lbcfg VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
add_subdirectory(lbmbc)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        GenerateVersion.cmake
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

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(lbcfg
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
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
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET lbcfg APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(lbcfg SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(lbcfg
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(lbcfg PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    lbmbc_lib)

# # Windows uses the stock QtSerialBus from the selected Qt installation.
# # This deliberately overwrites any stale patched Qt6SerialBus.dll left in the
# # build directory by older revisions, so runtime behaviour no longer depends
# # on PATH tricks or a special launcher.
# if(WIN32)
#     add_custom_command(TARGET lbcfg POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                 "$<TARGET_FILE:Qt${QT_VERSION_MAJOR}::SerialBus>"
#                 "$<TARGET_FILE_DIR:lbcfg>"
#         COMMENT "Deploying stock Qt SerialBus runtime"
#     )
# endif()

# === АВТО-ВЕРСИЯ: Подключаем сгенерированные заголовочные файлы к lbcfg ===
target_include_directories(lbcfg PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(lbcfg lbcfg_version_check)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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

include(GNUInstallDirs)
install(TARGETS lbcfg
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(lbcfg)
endif()
