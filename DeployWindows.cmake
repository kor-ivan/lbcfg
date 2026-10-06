# =============================================================================
#  WINDOWS-SPECIFIC DEPLOYMENT & INSTALLER PACKAGING SCRIPTS
# =============================================================================
include(GNUInstallDirs)

set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/installed_app")

# 1. Правила локальной инсталляции бинарников в installed_app
install(TARGETS lbcfg lbcfg_lib lbmbc_lib
    BUNDLE DESTINATION "${DEPLOY_DIR}"
    LIBRARY DESTINATION "${DEPLOY_DIR}"
    RUNTIME DESTINATION "${DEPLOY_DIR}"
)

if(TARGET lbmbc)
    install(TARGETS lbmbc RUNTIME DESTINATION "${DEPLOY_DIR}")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(lbcfg)
endif()

# 2. Глубокий деплой зависимостей через windeployqt
get_target_property(QT_BINARY_DIR Qt${QT_VERSION_MAJOR}::Core LOCATION)
get_filename_component(QT_BINARY_DIR "${QT_BINARY_DIR}" DIRECTORY)

add_custom_command(TARGET lbcfg POST_BUILD
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_CURRENT_BINARY_DIR}" --config Release --prefix "${DEPLOY_DIR}"
    COMMAND "${QT_BINARY_DIR}/windeployqt.exe" "${DEPLOY_DIR}/lbcfg.exe"
    COMMAND "${QT_BINARY_DIR}/windeployqt.exe" "${DEPLOY_DIR}/liblbmbc.dll"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEPLOY_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEPLOY_DIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEPLOY_DIR}/share"
    COMMENT "Running comprehensive deployment, deep windeployqt scanning, and cleaning up build garbage..."
)

# 3. Автоматизированная сборка инсталлятора через Qt Installer Framework (QtIFW)
if(ENABLE_INSTALLER_BUILD)
    set(VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.h")
    if(EXISTS "${VERSION_FILE}")
        file(READ "${VERSION_FILE}" VERSION_CONTENT)
        if(VERSION_CONTENT MATCHES "#define APP_VERSION_STRING \"([^\"]+)\"")
                set(FULL_VERSION_STR "${CMAKE_MATCH_1}")
            endif()
        endif()

        if(NOT FULL_VERSION_STR)
            set(FULL_VERSION_STR "${PROJECT_VERSION}")
        endif()

        string(REPLACE " " "_" SAFE_VERSION_STR "${FULL_VERSION_STR}")
        set(INSTALLER_FILENAME "setup_lbcfg_${SAFE_VERSION_STR}_win_x64.exe")

        find_program(IFW_BINARY_CREATOR
            NAMES binarycreator binarycreator.exe
            HINTS
            "C:/Qt/Tools/QtInstallerFramework/*/bin"  # Звездочка заставляет CMake сканировать ВСЕ версии папок
            "D:/Qt/Tools/QtInstallerFramework/*/bin"
            "C:/Qt6/Tools/QtInstallerFramework/*/bin"
            DOC "Path to the QtIFW binarycreator tool"
        )

    # Страховочная проверка: если утилита вообще не найдена в системе, пишем понятную ошибку
    if(NOT IFW_BINARY_CREATOR)
        message(FATAL_ERROR "
            [ERROR] Qt Installer Framework tool 'binarycreator' NOT FOUND!
            Please make sure it is installed via Qt Maintenance Tool (under Tools section).
            ")
    else()
        message(STATUS "Found QtIFW binarycreator automatically: ${IFW_BINARY_CREATOR}")
    endif()

    set(INSTALLER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/installer")
    set(INSTALLER_DATA_DIR "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/data")

    # -----------------------------------------------------------------
    # РЕШЕНИЕ: Создаем изолированный CMake-скрипт для текстового патча.
    # Никаких кавычек и многострочных блоков внутри Ninja больше нет!
    # -----------------------------------------------------------------
    set(PATCH_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/patch_config.cmake")
    file(WRITE "${PATCH_SCRIPT_FILE}" "
        file(READ \"${INSTALLER_CONFIG_DIR}/config/config.xml.in\" CONTENT)
        string(REPLACE \"@FULL_VERSION_STR@\" \"${FULL_VERSION_STR}\" CONTENT \"\${CONTENT}\")
        string(REPLACE \"%ApplicationsDirX64%\" \"@ApplicationsDirX64@\" CONTENT \"\${CONTENT}\")
        file(WRITE \"${INSTALLER_CONFIG_DIR}/config/config.xml\" \"\${CONTENT}\")
        ")
    # -----------------------------------------------------------------

    add_custom_command(
        OUTPUT "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${INSTALLER_DATA_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLER_DATA_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${DEPLOY_DIR}" "${INSTALLER_DATA_DIR}"

        COMMAND ${CMAKE_COMMAND} -P "${PATCH_SCRIPT_FILE}"

        COMMAND "${IFW_BINARY_CREATOR}" -c "${INSTALLER_CONFIG_DIR}/config/config.xml" -p "${INSTALLER_CONFIG_DIR}/packages" "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}"
        DEPENDS
        lbcfg
        #"${INSTALLER_CONFIG_DIR}/config/config.xml"
        "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/meta/package.xml"
        "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/meta/license.txt"
        "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/meta/installscript.qs"
        COMMENT "GENERATING IN THE BACKEND ARCHIVE INSTALLER: ${INSTALLER_FILENAME}..."
    )

add_custom_target(run_binarycreator_target ALL DEPENDS "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}")
endif()
