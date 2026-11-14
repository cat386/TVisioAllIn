#ifndef TVSTUDIO_CODEGEN_H
#define TVSTUDIO_CODEGEN_H
#include <tvstudio/model.h>
namespace tvstudio
{
struct GeneratedFile
{
    fs::path path;
    bool changed = false;
};
std::vector<GeneratedFile> generate(Project &project);
Project importProject(const fs::path &source, const fs::path &destination, const std::string &name,
                      const fs::path &library);
} // namespace tvstudio
#endif
