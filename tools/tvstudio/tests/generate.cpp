#include <iostream>
#include <tvstudio/codegen.h>
int main(int argc, char **argv)
{
    try
    {
        if (argc != 3)
            return 2;
        auto project = tvstudio::Project::create(
            "generated_studio", tvstudio::pathFromUtf8(argv[1]), tvstudio::pathFromUtf8(argv[2]));
        auto &f = project.forms.front();
        f.w = 110;
        f.h = 90;
        f.controls.clear();
        int y = 1;
        for (auto &k : tvstudio::kinds())
        {
            auto c = tvstudio::Control::make(k.kind, std::string(k.prefix) + "1", 2, y);
            if (c.kind == tvstudio::Kind::Label)
                c.link = "input1";
            y += c.h + 1;
            f.controls.push_back(c);
        }
        auto window = f;
        window.name = "window_form";
        window.window = true;
        project.forms.push_back(window);
        tvstudio::generate(project);
        std::cout << "Сгенерирован проект со всеми компонентами\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
