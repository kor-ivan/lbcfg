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
}
