#include <internal/terminfo.h>
#ifdef _TV_UNIX
#include <test.h>
using tvision::TermInfo;
TEST(TermInfo, FormatsWideFieldsWithoutTruncation) {
    int params[]={7};
    EXPECT_EQ(TermInfo::tparm("%p1%80d",params,1),std::string(79,' ')+"7");
    EXPECT_EQ(TermInfo::tparm("%p1%080d",params,1),std::string(79,'0')+"7");
    EXPECT_EQ(TermInfo::tparm("%p1%:80.4d",params,1),std::string(76,' ')+"0007");
}
TEST(TermInfo, RetainsShortFormattingAndConditionals) {
    int params[]={7};
    EXPECT_EQ(TermInfo::tparm("%p1%d",params,1),"7");
    EXPECT_EQ(TermInfo::tparm("%?%p1%{8}%<%tLOW%eHIGH%;",params,1),"LOW");
}
TEST(TermInfo, BoundsExpandedFields) {
    int params[]={7};
    EXPECT_EQ(TermInfo::tparm("%p1%1048577d",params,1),"");
    EXPECT_EQ(TermInfo::tparm("%p1%2147483647d",params,1),"");
    EXPECT_EQ(TermInfo::tparm("%p1%:.2147483647d",params,1),"");
}

namespace {
void word(std::string &out, unsigned value) {
    out += char(value & 255);
    out += char((value >> 8) & 255);
}
std::string entry(bool wide = false) {
    std::string out;
    for (unsigned n : {wide ? 01036u : 0432u, 2u, 2u, 1u, 1u, 4u})
        word(out, n);
    out.append("x\0\0\1", 4); // name, bw=false, am=true; already aligned.
    word(out, 80);
    if (wide)
        word(out, 0);
    word(out, 0); // cbt string offset.
    out.append("abc\0", 4);
    return out;
}
}
TEST(TermInfo, ParsesLegacyAnd32BitRecords) {
    for (bool wide : {false, true}) {
        auto data = entry(wide);
        TermInfo ti;
        ASSERT_TRUE(TermInfo::parse(data.data(), data.size(), ti));
        EXPECT_TRUE(ti.getBool("am"));
        EXPECT_FALSE(ti.getBool("bw"));
        EXPECT_EQ(ti.getNum("cols"), 80);
        ASSERT_TRUE(ti.getStr("cbt") != nullptr);
        EXPECT_EQ(std::string(ti.getStr("cbt")), "abc");
        for (size_t n = 0; n < data.size(); ++n) {
            TermInfo truncated;
            EXPECT_FALSE(TermInfo::parse(data.data(), n, truncated));
        }
    }
}
TEST(TermInfo, RejectsInvalidMagicAndNegativeSectionLengths) {
    for (size_t field = 0; field < 6; ++field) {
        auto data = entry();
        data[field * 2] = char(0xff);
        data[field * 2 + 1] = char(0xff);
        TermInfo ti;
        EXPECT_FALSE(TermInfo::parse(data.data(), data.size(), ti));
    }
}
TEST(TermInfo, IgnoresOutOfBoundsAndUnterminatedStrings) {
    for (unsigned offset : {4u, 32767u, 65535u, 65534u}) {
        auto data = entry();
        data[18] = char(offset & 255);
        data[19] = char(offset >> 8);
        TermInfo ti;
        ASSERT_TRUE(TermInfo::parse(data.data(), data.size(), ti));
        EXPECT_TRUE(ti.getStr("cbt") == nullptr);
        EXPECT_EQ(ti.getNum("cols"), 80);
    }
    auto data = entry();
    data.back() = 'x';
    TermInfo ti;
    ASSERT_TRUE(TermInfo::parse(data.data(), data.size(), ti));
    EXPECT_TRUE(ti.getStr("cbt") == nullptr);
}
TEST(TermInfo, MalformedExtensionsPreserveBaseCapabilities) {
    auto data = entry();
    // A valid extension header claiming a missing boolean and string table.
    for (unsigned n : {1u, 0u, 0u, 1u, 4u})
        word(data, n);
    TermInfo ti;
    ASSERT_TRUE(TermInfo::parse(data.data(), data.size(), ti));
    EXPECT_TRUE(ti.getBool("am"));
    EXPECT_EQ(ti.getNum("cols"), 80);
    ASSERT_TRUE(ti.getStr("cbt") != nullptr);
    EXPECT_EQ(std::string(ti.getStr("cbt")), "abc");
}
#endif
