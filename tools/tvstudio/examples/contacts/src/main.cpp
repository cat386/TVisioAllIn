// Пользовательская логика: генератор сохраняет этот файл при обновлении форм.
#include "forms/forms.h"
#include "ui.h"

class Contacts : public tvision::TThemedApplication
{
    forms::contact::Data contact_;

  public:
    Contacts() : TProgInit(&ui::statusLine, &ui::menuBar, &TApplication::initDeskTop)
    {
        // При первом запуске используются значения из модели формы.
        try
        {
            auto j = tvstudio::parseDocument(tvstudio::readFile("contact.json"));
            contact_.full_name = tvstudio::str(j, "full_name", contact_.full_name);
            contact_.category = tvstudio::str(j, "category", contact_.category);
            contact_.notes = tvstudio::str(j, "notes", contact_.notes);
            contact_.flags = ushort(tvstudio::integer(j, "flags", contact_.flags));
            if (tvstudio::get(j, "phones").kind == tvstudio::Json::Kind::Array)
            {
                contact_.phones.clear();
                for (auto &row : tvstudio::get(j, "phones").array)
                {
                    std::vector<std::string> cells;
                    for (auto &cell : row.array)
                        cells.push_back(cell.display());
                    contact_.phones.push_back(std::move(cells));
                }
            }
        }
        catch (const std::exception &error)
        {
            if (tvstudio::fs::exists("contact.json"))
                messageBox(error.what(), mfError | mfOKButton);
        }
    }

    void handleEvent(TEvent &event) override
    {
        TThemedApplication::handleEvent(event);
        if (event.what == evCommand && event.message.command == cmd::EDIT_CONTACT)
        {
            if (forms::contact::run(*this, contact_) == cmOK)
            {
                try
                {
                    // Один способ сериализации для всех типов полей, включая таблицу.
                    auto *view = forms::contact::build(contact_);
                    auto data = view->binding.read();
                    TObject::destroy(view);
                    tvstudio::writeFile("contact.json", tvision::writeJson(data, true));
                }
                catch (const std::exception &error)
                {
                    messageBox(error.what(), mfError | mfOKButton);
                }
            }
            clearEvent(event);
        }
    }
};

int main()
{
    Contacts app;
    app.run();
    app.shutDown();
}
