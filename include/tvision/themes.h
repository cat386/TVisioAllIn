#ifndef TVISION_THEMES_H
#define TVISION_THEMES_H
#define Uses_TPalette
#define Uses_TApplication
#define Uses_TScreen
#define Uses_TColorAttr
#include <array>
#include <string>
#include <tvision/tv.h>
namespace tvision
{
enum class Theme
{
    Classic,
    Gruvbox,
    Twilight
};
const char *themeName(Theme theme) noexcept;
TColorAttr color256(unsigned char foreground, unsigned char background) noexcept;
unsigned char contrast256(unsigned char color) noexcept;
std::array<TColorAttr, 135> themeColors(Theme theme, bool extended = true);
class ThemePalette
{
    TPalette palette_;

  public:
    explicit ThemePalette(Theme theme = Theme::Gruvbox, bool extended = true);
    void setTheme(Theme theme, bool extended = true);
    TPalette &palette() const
    {
        return const_cast<TPalette &>(palette_);
    }
};
class TThemedApplication : public TApplication
{
    Theme theme_;
    ThemePalette palette_;

  public:
    explicit TThemedApplication(Theme theme = Theme::Gruvbox);
    void setTheme(Theme theme);
    Theme theme() const noexcept
    {
        return theme_;
    }
    TPalette &getPalette() const override;
};
} // namespace tvision
#endif
