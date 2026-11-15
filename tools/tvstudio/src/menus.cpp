#include "studio_ui.h"

namespace tvstudio
{
Application studioMenus()
{
    Application a;
    auto item = [](const char *title, ushort command, const char *key = "")
    { return Menu{title, std::to_string(command), key, 0, false, {}}; };
    a.menu = {
        {"~Ф~айл",
         "",
         "",
         0,
         false,
         {item("Новый проект...", NewProject), item("Новый исходник", NewSource, "Ctrl+N"),
          item("Открыть проект...", OpenProject, "Ctrl+O"), item("Импорт C++ / CMake...", Import),
          item("Открыть исходник...", OpenFile), item("Сохранить", Save, "F2"),
          item("Сохранить всё", SaveAll, "Ctrl+S"), item("Выход", cmQuit, "Alt+X")}},
        {"~П~равка",
         "",
         "",
         0,
         false,
         {item("Отменить", cmUndo, "Ctrl+Z"), item("Повторить", Redo),
          item("Вырезать", cmCut, "Ctrl+X"), item("Копировать", cmCopy, "Ctrl+C"),
          item("Вставить", cmPaste, "Ctrl+V"), item("Найти...", Find, "Ctrl+F"),
          item("Заменить...", Replace, "Ctrl+R"), item("Строка...", Goto, "Ctrl+G"),
          item("Дополнить слово", Complete, "Ctrl+Space")}},
        {"Ф~о~рма",
         "",
         "",
         0,
         false,
         {item("Новая форма...", NewForm), item("Компоненты... (Insert)", Palette),
          item("Свойства... (Enter)", Properties), item("Предпросмотр", Preview, "F9"),
          item("Выровнять слева", AlignLeft), item("Выровнять сверху", AlignTop),
          item("Одинаковая ширина", AlignWidth), item("Раньше в Tab-порядке", TabBack),
          item("Позже в Tab-порядке", TabForward)}},
        {"Прое~к~т",
         "",
         "",
         0,
         false,
         {item("Дерево проекта", ProjectTree, "F3"),
          item("Меню, команды, строка состояния...", AppModel), item("Генерировать C++", Generate),
          item("Настройки сборки...", BuildOptionsCommand),
          item("clangd: команда запуска...", Clangd),
          item("CMake: обновить данные для clangd", ConfigureLsp),
          item("Определение символа", Definition, "F12"),
          item("Использования символа", References)}},
        {"~С~борка",
         "",
         "",
         0,
         false,
         {item("Собрать", Build, "F7"), item("Собрать и запустить", Run, "Ctrl+F9"),
          item("Тесты", Test), item("Очистить", Clean), item("Остановить", Stop, "Ctrl+F2"),
          item("Следующая ошибка", NextError, "F8")}},
        {"~О~кна",
         "",
         "",
         0,
         false,
         {item("Редактор кода", CodeEditor, "F4"), item("Дизайнер форм", FormDesigner, "Shift+F4"),
          item("Следующее", cmNext, "F6"), item("Мозаика", cmTile), item("Каскад", cmCascade),
          item("Тема", ThemeCommand)}},
        {"~?~",
         "",
         "",
         0,
         false,
         {item("Работа в TV Studio", Help, "F1"), item("О программе", About)}}};
    // Имена локальных команд разрешаются общим построителем меню.
    for (unsigned c = NewProject; c <= Help; ++c)
        a.commands.push_back({std::to_string(c), "", "", c});
    for (ushort c : {cmQuit, cmUndo, cmCut, cmCopy, cmPaste, cmNext, cmTile, cmCascade})
        a.commands.push_back({std::to_string(c), "", "", c});
    a.status = {{"~F1~ Помощь", "F1", std::to_string(Help)},
                {"~F2~ Сохранить", "F2", std::to_string(Save)},
                {"~F7~ Сборка", "F7", std::to_string(Build)},
                {"~F9~ Форма", "F9", std::to_string(Preview)},
                {"~Alt+X~ Выход", "Alt+X", std::to_string(cmQuit)}};
    return a;
}

} // namespace tvstudio
