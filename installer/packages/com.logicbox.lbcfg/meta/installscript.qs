function Component() {
    // Конструктор компонента
}

Component.prototype.createOperations = function() {
    // Вызываем стандартный процесс копирования файлов
    component.createOperations();

    // Проверяем, что установка идет на Windows
    if (systemInfo.productType === "windows") {
        // Создаем ярлык в меню «Пуск»
        // Аргументы: Исходный файл, Путь к ярлыку, Рабочая директория (необязательно)
        component.addOperation("CreateShortcut",
                               installer.value("TargetDir") + "/lbcfg.exe",
                               installer.value("StartMenuDir") + "/LogicBox Configuration Tool.lnk");

        // (Опционально) Если хотите ярлык еще и на Рабочий стол, раскомментируйте строку ниже:
        component.addOperation("CreateShortcut", installer.value("TargetDir") + "/lbcfg.exe", installer.value("DesktopDir") + "/LogicBox.lnk");
        // Ярлык для удаления программы
        component.addOperation("CreateShortcut",
                                       installer.value("TargetDir") + "/maintenancetool.exe",
                                       installer.value("StartMenuDir") + "/Удалить LogicBox Tool.lnk",
                                       "--uninstall");
    }
    else if (systemInfo.productType === "linux" || systemInfo.productType === "ubuntu" || systemInfo.productType === "debian") {
        // --- ДЛЯ LINUX (DEBIAN) ---

        // 1. Создаем ярлык для главного приложения (добавлен префикс LogicBox)
        component.addOperation("CreateDesktopEntry",
            "com.logicbox.lbcfg.desktop",
            "Type=Application\n" +
            "Name=LogicBox: Configuration Tool\n" +
            "Exec=\"@TargetDir@/lbcfg\"\n" +
            "Comment=LogicBox Configuration Tool\n" +
            "Icon=@TargetDir@/share/icons/hicolor/256x256/apps/logo.png\n" +
            "Categories=Utility;Development;\n" +
            "Terminal=false"
        );

        // 2. Создаем ярлык для утилиты удаления (добавлен префикс LogicBox)
        component.addOperation("CreateDesktopEntry",
            "com.logicbox.lbcfg-uninstall.desktop",
            "Type=Application\n" +
            "Name=LogicBox: Uninstall Tool\n" +
            "Exec=\"@TargetDir@/maintenancetool\"\n" +
            "Comment=Uninstall LogicBox Configuration Tool\n" +
            "Icon=@TargetDir@/share/icons/hicolor/256x256/apps/logo.png\n" +
            "Categories=Utility;Development;\n" +
            "Terminal=false"
        );
    }
}
