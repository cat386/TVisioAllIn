#ifndef TVSTUDIO_UI_H
#define TVSTUDIO_UI_H

#include <tvstudio/runtime.h>

namespace tvstudio
{
enum : ushort
{
    NewProject = 500,
    OpenProject,
    OpenFile,
    Save,
    SaveAll,
    NewForm,
    Properties,
    Preview,
    Generate,
    Build,
    Run,
    Test,
    Clean,
    Stop,
    ProjectTree,
    Palette,
    AppModel,
    BuildOptionsCommand,
    Find,
    Replace,
    Goto,
    Complete,
    ThemeCommand,
    About,
    Redo,
    AlignLeft,
    AlignTop,
    AlignWidth,
    TabForward,
    TabBack,
    Import,
    NextError,
    Definition,
    References,
    Clangd,
    CodeEditor,
    FormDesigner,
    NewSource,
    ConfigureLsp,
    Help
};

Application studioMenus();
} // namespace tvstudio

#endif
