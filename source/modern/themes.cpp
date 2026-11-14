// Theme indices and Gruvbox palette ported from cat386/tvision-rs (MIT).
#include <algorithm>
#include <tvision/themes.h>
namespace tvision
{
TColorAttr color256(unsigned char fg, unsigned char bg) noexcept
{
    return {TColorXTerm(fg), TColorXTerm(bg)};
}
unsigned char contrast256(unsigned char color) noexcept
{
    static const uint32_t base[16] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
                                      0x008080, 0xC0C0C0, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
                                      0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF};
    TColorRGB rgb =
        color < 16 ? TColorRGB(base[color]) : TColorConversion::XTerm256toRGB(TColorXTerm(color));
    return (unsigned(rgb.getRed()) * 299 + unsigned(rgb.getGreen()) * 587 +
            unsigned(rgb.getBlue()) * 114) >= 128000
               ? 0
               : 15;
}
const char *themeName(Theme t) noexcept
{
    return t == Theme::Gruvbox ? "Gruvbox" : t == Theme::Twilight ? "Twilight" : "Classic";
}
std::array<TColorAttr, 135> themeColors(Theme theme, bool extended)
{
    static const unsigned char classic[] = cpAppColor;
    static const unsigned char dusk[16] = {235, 24, 71,  30,  131, 96,  137, 252,
                                           242, 74, 114, 116, 210, 176, 222, 255};
    static const unsigned char gruvbox[135][2] = {
        {237, 234}, {223, 237}, {245, 237}, {208, 237}, {235, 214}, {241, 214}, {124, 214},
        {243, 235}, {223, 235}, {142, 235}, {241, 237}, {246, 237}, {223, 235}, {235, 109},
        {229, 235}, {243, 237}, {223, 237}, {108, 237}, {241, 239}, {246, 239}, {223, 237},
        {235, 108}, {229, 237}, {243, 239}, {223, 239}, {214, 239}, {241, 237}, {246, 237},
        {223, 239}, {235, 214}, {229, 239}, {243, 237}, {223, 237}, {142, 237}, {241, 239},
        {246, 239}, {223, 237}, {250, 237}, {214, 237}, {208, 237}, {223, 241}, {108, 241},
        {214, 241}, {245, 239}, {208, 241}, {234, 237}, {250, 237}, {214, 237}, {208, 237},
        {223, 239}, {235, 108}, {142, 239}, {235, 142}, {142, 237}, {241, 239}, {246, 239},
        {223, 239}, {235, 109}, {214, 239}, {241, 239}, {108, 239}, {245, 237}, {223, 237},
        {243, 235}, {223, 235}, {142, 235}, {241, 237}, {246, 237}, {223, 235}, {250, 235},
        {214, 235}, {208, 235}, {223, 241}, {108, 241}, {214, 241}, {245, 237}, {208, 241},
        {234, 235}, {250, 235}, {214, 235}, {208, 235}, {223, 237}, {235, 108}, {142, 237},
        {235, 142}, {142, 235}, {241, 237}, {246, 237}, {223, 237}, {235, 109}, {214, 237},
        {241, 237}, {108, 237}, {245, 235}, {223, 235}, {243, 239}, {223, 239}, {142, 239},
        {241, 241}, {246, 241}, {223, 239}, {250, 239}, {214, 239}, {208, 239}, {223, 243},
        {108, 243}, {214, 243}, {245, 241}, {208, 243}, {234, 239}, {250, 239}, {214, 239},
        {208, 239}, {223, 241}, {235, 108}, {142, 241}, {235, 142}, {142, 239}, {241, 241},
        {246, 241}, {223, 241}, {235, 109}, {214, 241}, {241, 241}, {108, 241}, {245, 239},
        {223, 239}, {243, 237}, {223, 237}, {108, 237}, {241, 239}, {246, 239}, {223, 237},
        {214, 237}, {235, 214},
    };
    std::array<TColorAttr, 135> colors;
    for (size_t i = 0; i < colors.size(); ++i)
    {
        if (theme == Theme::Classic)
            colors[i] = TColorAttr(classic[i]);
        else if (theme == Theme::Twilight)
            colors[i] = color256(dusk[classic[i] & 15], dusk[classic[i] >> 4]);
        else
            colors[i] = color256(gruvbox[i][0], gruvbox[i][1]);
        if (!extended)
            colors[i] = TColorAttr(colors[i].toBIOS());
    }
    return colors;
}
ThemePalette::ThemePalette(Theme t, bool extended) : palette_(cpAppColor, 135)
{
    setTheme(t, extended);
}
void ThemePalette::setTheme(Theme t, bool extended)
{
    auto colors = themeColors(t, extended);
    for (size_t i = 0; i < colors.size(); ++i)
        palette_[int(i + 1)] = colors[i];
}
void refreshTheme(TGroup &root)
{
    bool exposed = (root.state & sfExposed) != 0;
    root.setState(sfExposed, False);
    if (exposed)
    {
        root.setState(sfExposed, True);
        if (root.options & ofBuffered)
            root.drawView();
        else
            root.redraw();
    }
}
TThemedApplication::TThemedApplication(Theme t)
    : TProgInit(&TApplication::initStatusLine, &TApplication::initMenuBar,
                &TApplication::initDeskTop),
      theme_(t), palette_(t, (TScreen::screenMode & TDisplay::smColor256) != 0)
{
    // The desktop and menus were first painted by TProgram's constructor,
    // before virtual getPalette() could dispatch to this class.
    refreshTheme(*this);
}
TPalette &TThemedApplication::getPalette() const
{
    return palette_.palette();
}
void TThemedApplication::setTheme(Theme t)
{
    theme_ = t;
    palette_.setTheme(t, (TScreen::screenMode & TDisplay::smColor256) != 0);
    refreshTheme(*this);
}
} // namespace tvision
