#define Uses_TFileEditor
#define Uses_TEditor
#define Uses_TRect
#include <tvision/tv.h>
#include <internal/filesave.h>
#include <test.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs = std::filesystem;
namespace {
class FileSave : public testing::Test {
protected:
    fs::path dir;
    void SetUp() override {
        static unsigned counter = 0;
        dir = fs::temp_directory_path() / ("tvisio-save-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(counter++));
        fs::create_directory(dir);
    }
    void TearDown() override { fs::remove_all(dir); }
    static void put(const fs::path &p, const char *text) { std::ofstream(p, std::ios::binary) << text; }
    static std::string get(const fs::path &p) { std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f), {}}; }
    static std::string utf8(const fs::path &p) {
        auto s=p.u8string();return {reinterpret_cast<const char *>(s.data()),s.size()};
    }
    bool save(const fs::path &p, const fs::path &backup = {}) {
        auto target=utf8(p), b=utf8(backup);
        return tvision::saveFileAtomic(target.c_str(), b.empty()?nullptr:b.c_str(), "new",3," content",8);
    }
};
TEST_F(FileSave, CreatesNewFileFromBothBufferParts) {
    auto p=dir/"a.txt";ASSERT_TRUE(save(p));EXPECT_EQ(get(p),"new content");
    EXPECT_EQ(std::distance(fs::directory_iterator(dir),fs::directory_iterator()),1);
}
TEST_F(FileSave, ReplacesFileAndRotatesBackup) {
    auto p=dir/"a.txt",b=dir/"a.bak";put(p,"original");put(b,"previous backup");
    ASSERT_TRUE(save(p,b));EXPECT_EQ(get(p),"new content");EXPECT_EQ(get(b),"original");
}
TEST_F(FileSave, ReplacesWithoutBackup) {
    auto p=dir/"a.txt";put(p,"original");ASSERT_TRUE(save(p));EXPECT_EQ(get(p),"new content");
    EXPECT_EQ(std::distance(fs::directory_iterator(dir),fs::directory_iterator()),1);
}
TEST_F(FileSave, BackupFailurePreservesOriginalAndCleansTemporaryFile) {
    auto p=dir/"a.txt",b=dir/"backup";put(p,"original");fs::create_directory(b);put(b/"keep","keep");
    EXPECT_FALSE(save(p,b));EXPECT_EQ(get(p),"original");EXPECT_EQ(get(b/"keep"),"keep");
    EXPECT_EQ(std::distance(fs::directory_iterator(dir),fs::directory_iterator()),2);
}
TEST_F(FileSave, BackupMayHaveTheSameNameAsOriginal) {
    auto p=dir/"a.bak";put(p,"original");ASSERT_TRUE(save(p,p));
    EXPECT_EQ(get(p),"new content");EXPECT_EQ(get(dir/"a.bak.bak"),"original");
}
TEST_F(FileSave, SupportsUnicodeFileNames) {
    auto p=dir/fs::u8path(u8"тест.txt"),b=dir/fs::u8path(u8"тест.bak");put(p,"original");
    ASSERT_TRUE(save(p,b));EXPECT_EQ(get(p),"new content");EXPECT_EQ(get(b),"original");
}
#ifdef _WIN32
TEST_F(FileSave, DeniedRenamePreservesOriginalBackupAndModifiedFlag) {
    auto p=dir/"a.txt",b=dir/"a.bak";put(p,"original");put(b,"previous backup");
    auto name=utf8(p);
    auto *editor=new TFileEditor(TRect(),nullptr,nullptr,nullptr,name.c_str());
    editor->setSelect(0,8,False);editor->insertText("changed",7,False);
    HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    EXPECT_NE(h,INVALID_HANDLE_VALUE);
    EXPECT_FALSE(editor->saveFile());EXPECT_TRUE(editor->modified);
    CloseHandle(h);
    EXPECT_EQ(get(p),"original");EXPECT_EQ(get(b),"previous backup");
    editor->shutDown();delete editor;
}
#else
TEST_F(FileSave, PreservesSymbolicLinkAndPermissions) {
    auto p=dir/"a.txt",link=dir/"link.txt";put(p,"original");
    fs::permissions(p,fs::perms::owner_read|fs::perms::owner_write);
    fs::create_symlink(p,link);ASSERT_TRUE(save(link));
    EXPECT_TRUE(fs::is_symlink(link));EXPECT_EQ(get(p),"new content");
    EXPECT_EQ(fs::status(p).permissions(),fs::perms::owner_read|fs::perms::owner_write);
}
#endif
}
