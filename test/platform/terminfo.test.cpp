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
#endif
