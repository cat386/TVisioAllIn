#ifndef TVSTUDIO_RUNTIME_H
#define TVSTUDIO_RUNTIME_H
#define Uses_TWindow
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TLabel
#define Uses_TInputLine
#define Uses_TCheckBoxes
#define Uses_TRadioButtons
#define Uses_TSItem
#define Uses_TListBox
#define Uses_TStringCollection
#define Uses_TMemo
#define Uses_THistory
#define Uses_TScrollBar
#define Uses_TValidator
#define Uses_TPXPictureValidator
#define Uses_TFilterValidator
#define Uses_TStringLookupValidator
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TDeskTop
#define Uses_TEditWindow
#define Uses_TFileEditor
#define Uses_MsgBox
#include <tvision/modern.h>
#include <tvision/tv.h>
#include <tvstudio/model.h>

namespace tvstudio
{
TKey parseKey(const std::string &text);
ushort commandCode(const Application &app, const std::string &name);
TMenuBar *makeMenuBar(TRect bounds, const Application &app);
TStatusLine *makeStatusLine(TRect bounds, const Application &app);
std::string editorText(TEditor &editor);
void replaceEditorText(TEditor &editor, const std::string &text);

class FormBinding
{
    TWindow *owner_ = nullptr;
    Form form_;
    std::map<std::string, TView *> controls_;

  public:
    void build(TWindow &owner, const Form &form, const Json &data, const Application &app);
    TView *control(const std::string &name) const;
    Json read() const;
    void write(const Json &data);
    const Form &model() const
    {
        return form_;
    }
};
using CommandHandler = std::function<void(ushort, TWindow &)>;
class FormDialog : public TDialog
{
  public:
    FormBinding binding;
    CommandHandler onCommand;
    FormDialog(const Form &form, const Json &data = {}, const Application &app = {});
    void handleEvent(TEvent &event) override;
};
class FormWindow : public TWindow
{
  public:
    FormBinding binding;
    CommandHandler onCommand;
    FormWindow(const Form &form, const Json &data = {}, const Application &app = {});
    TPalette &getPalette() const override;
    void close() override;
    void handleEvent(TEvent &event) override;
};
TWindow *makeForm(const Form &form, const Json &data = {}, const Application &app = {},
                  CommandHandler onCommand = {});
Json readForm(TWindow &view);
ushort previewForm(TProgram &app, const Form &form, Json &data, const Application &model = {});
std::vector<std::shared_ptr<tvision::TreeNode>> treeNodes(const Control &control);
std::shared_ptr<tvision::VecTableModel> tableModel(const Control &control, const Json &data = {});
} // namespace tvstudio
#endif
