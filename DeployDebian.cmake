# =============================================================================
#  LINUX (DEBIAN) DEPLOYMENT VIA AUTOMATED LINUXDEPLOYQT
# =============================================================================

set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/installed_app")

find_program(LINUXDEPLOYQT_EXECUTABLE NAMES linuxdeployqt)

if(NOT LINUXDEPLOYQT_EXECUTABLE)
    set(DOWNLOAD_PATH "${CMAKE_BINARY_DIR}/tools/linuxdeployqt")
    if(NOT EXISTS "${DOWNLOAD_PATH}")
        file(DOWNLOAD "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
            "${DOWNLOAD_PATH}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
    execute_process(COMMAND chmod +x "${DOWNLOAD_PATH}")
endif()
set(LINUXDEPLOYQT_EXECUTABLE "${DOWNLOAD_PATH}")
endif()

install(TARGETS lbcfg lbcfg_lib lbmbc_lib
    BUNDLE DESTINATION "${DEPLOY_DIR}"
    LIBRARY DESTINATION "${DEPLOY_DIR}"
    RUNTIME DESTINATION "${DEPLOY_DIR}"
)

if(TARGET lbmbc)
    install(TARGETS lbmbc RUNTIME DESTINATION "${DEPLOY_DIR}")
endif()

set(CURRENT_QMAKE_EXE "${QT_QMAKE_EXECUTABLE}")

add_custom_command(TARGET lbcfg POST_BUILD
    # Шаг 1: Стандартная инсталляция бинарников
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_CURRENT_BINARY_DIR}" --config Release --prefix "${DEPLOY_DIR}"

    # Шаг 2: Создание структуры папок под Linux-иконку
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/share/icons/hicolor/256x256/apps"

    # Шаг 3: Исправлен путь — забираем logo.png прямо из папки resources/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.png" "${DEPLOY_DIR}/share/icons/hicolor/256x256/apps/logo.png"

    # Шаг 4: Запуск деплоера Qt
    COMMAND "${LINUXDEPLOYQT_EXECUTABLE}" "${DEPLOY_DIR}/lbcfg" -executable="${DEPLOY_DIR}/lbmbc" -qmake="${CURRENT_QMAKE_EXE}" -verbose=1 -unsupported-allow-new-glibc

    COMMENT "Running safe Linux deployment and copying application icon..."
)

# Настройка RPATH поиска локальных библиотек
set(LINUX_RPATH "\$ORIGIN;\$ORIGIN/lib")
set_target_properties(lbcfg lbcfg_lib PROPERTIES INSTALL_RPATH "${LINUX_RPATH}")
if(TARGET lbmbc_lib)
    set_target_properties(lbmbc_lib PROPERTIES INSTALL_RPATH "${LINUX_RPATH}")
endif()


# =============================================================================
#  АВТОМАТИЗИРОВАННАЯ СБОРКА ИНСТАЛЛЯТОРА ЧЕРЕЗ QtIFW ДЛЯ LINUX
# =============================================================================
if(ENABLE_INSTALLER_BUILD)
    # 1. Получаем динамическую версию из файла version.h (ваша оригинальная логика)
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

        # Имя инсталлятора для Linux (обычно имеет расширение .run)
        string(REPLACE " " "_" SAFE_VERSION_STR "${FULL_VERSION_STR}")
        set(INSTALLER_FILENAME "setup_lbcfg_${SAFE_VERSION_STR}_linux_x64.run")

        # 2. Поиск binarycreator в системе Linux
        find_program(IFW_BINARY_CREATOR
            NAMES binarycreator
            HINTS
            "$ENV{HOME}/Qt/Tools/QtInstallerFramework/*/bin"    # Сканирует любые версии папок в домашней папке Qt
            "$ENV{HOME}/Qt6/Tools/QtInstallerFramework/*/bin"
            "/opt/Qt/Tools/QtInstallerFramework/*/bin"          # Проверка глобальных путей установки
            DOC "Path to the Linux QtIFW binarycreator tool"
        )

    # Проверка на случай отсутствия утилиты в Linux
    if(NOT IFW_BINARY_CREATOR)
        message(FATAL_ERROR "
            [ERROR] Linux Qt Installer Framework tool 'binarycreator' NOT FOUND!
            Please make sure it is installed via Qt Maintenance Tool or in your $HOME/Qt path.
            ")
    else()
        message(STATUS "Found Linux QtIFW binarycreator automatically: ${IFW_BINARY_CREATOR}")
    endif()

    set(INSTALLER_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/installer")
    set(INSTALLER_DATA_DIR "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/data")

    # 3. Создаем изолированный CMake-скрипт для текстового патча config.xml под Linux
    set(PATCH_SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/patch_config.cmake")
    file(WRITE "${PATCH_SCRIPT_FILE}" "
        file(READ \"${INSTALLER_CONFIG_DIR}/config/config.xml.in\" CONTENT)
        string(REPLACE \"@FULL_VERSION_STR@\" \"${FULL_VERSION_STR}\" CONTENT \"\${CONTENT}\")
        # На Linux дефолтную папку установки ApplicationsDirX64 заменяем на домашний каталог пользователя
        string(REPLACE \"%ApplicationsDirX64%\" \"@HomeDir@/Applications\" CONTENT \"\${CONTENT}\")
        file(WRITE \"${INSTALLER_CONFIG_DIR}/config/config.xml\" \"\${CONTENT}\")
        ")

    # 4. Кастомная команда сборки инсталлятора (.run)
    add_custom_command(
        OUTPUT "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${INSTALLER_DATA_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${INSTALLER_DATA_DIR}"

        # Копируем созданный утилитой linuxdeployqt standalone-пакет в папку данных инсталлятора
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${DEPLOY_DIR}" "${INSTALLER_DATA_DIR}"

        # Запускаем патч конфигурационного XML
        COMMAND ${CMAKE_COMMAND} -P "${PATCH_SCRIPT_FILE}"

        # Собираем финальный кликабельный .run файл
        COMMAND "${IFW_BINARY_CREATOR}" -c "${INSTALLER_CONFIG_DIR}/config/config.xml" -p "${INSTALLER_CONFIG_DIR}/packages" "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}"

        DEPENDS
        lbcfg
        "${INSTALLER_CONFIG_DIR}/config/config.xml.in"
        "${INSTALLER_CONFIG_DIR}/packages/com.logicbox.lbcfg/meta/package.xml"
        COMMENT "GENERATING LINUX GRAPHICAL INSTALLER: ${INSTALLER_FILENAME}..."
    )

# Привязываем выполнение к общему таргету, чтобы инсталлятор собирался автоматически при Build
add_custom_target(run_binarycreator_target ALL DEPENDS "${INSTALLER_CONFIG_DIR}/${INSTALLER_FILENAME}")
endif()
