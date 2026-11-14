#include "tvtest.h"

#include <stdio.h>
#include <string.h>
#include <exception>

namespace testing
{
namespace internal
{

static bool currentTestFailed = false;

std::vector<TestInfo> &registry()
{
    static std::vector<TestInfo> tests;
    return tests;
}

void reportFailure(const char *file, int line, const std::string &summary, const std::string &message)
{
    currentTestFailed = true;
    std::string text = std::string(file) + ":" + std::to_string(line) + ": Failure\n" + summary;
    if (!message.empty())
        text += "\n" + message;
    fprintf(stdout, "%s\n", text.c_str());
    fflush(stdout);
}

void printBytes(const unsigned char *data, size_t size, std::ostream &os)
{
    static const char hex[] = "0123456789ABCDEF";
    os << size << "-byte object <";
    for (size_t i = 0; i < size; ++i)
    {
        if (i > 0)
            os << (i % 2 == 0 ? "  " : "-");
        os << hex[data[i] >> 4] << hex[data[i] & 0xF];
    }
    os << ">";
}

static void printEscaped(int ch, std::ostream &os, char quote)
{
    switch (ch)
    {
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        case '\0': os << "\\0"; break;
        case '\\': os << "\\\\"; break;
        default:
            if (ch == quote)
                os << '\\' << quote;
            else if (32 <= ch && ch < 127)
                os << (char) ch;
            else
            {
                static const char hex[] = "0123456789ABCDEF";
                os << "\\x" << hex[(ch >> 4) & 0xF] << hex[ch & 0xF];
            }
    }
}

void printChar(int ch, std::ostream &os)
{
    os << '\'';
    printEscaped(ch, os, '\'');
    os << "' (" << ch << ")";
}

void printCharArray(const char *data, size_t size, std::ostream &os)
{
    // Как GoogleTest, пропускаем завершающий нулевой символ, если он есть.
    if (size > 0 && data[size - 1] == '\0')
        --size;
    os << '"';
    for (size_t i = 0; i < size; ++i)
        printEscaped((unsigned char) data[i], os, '"');
    os << '"';
}

static bool matchesPattern(const char *pattern, const char *patternEnd, const char *str)
{
    // Поддерживает подстановочные символы '*' и '?'.
    if (pattern == patternEnd)
        return *str == '\0';
    if (*pattern == '*')
        return matchesPattern(pattern + 1, patternEnd, str) ||
               (*str != '\0' && matchesPattern(pattern, patternEnd, str + 1));
    if (*str != '\0' && (*pattern == '?' || *pattern == *str))
        return matchesPattern(pattern + 1, patternEnd, str + 1);
    return false;
}

static bool matchesAny(const char *patterns, const char *end, const char *str)
{
    // Шаблоны разделяются двоеточием.
    const char *begin = patterns;
    while (begin <= end)
    {
        const char *sep = begin;
        while (sep < end && *sep != ':')
            ++sep;
        if (sep > begin && matchesPattern(begin, sep, str))
            return true;
        begin = sep + 1;
    }
    return false;
}

static bool matchesFilter(const std::string &filter, const std::string &fullName)
{
    const char *f = filter.c_str();
    const char *dash = strchr(f, '-');
    const char *posEnd = dash ? dash : f + filter.size();
    bool positive = posEnd == f ? true : matchesAny(f, posEnd, fullName.c_str());
    bool negative = dash ? matchesAny(dash + 1, f + filter.size(), fullName.c_str()) : false;
    return positive && !negative;
}

int runAllTests(int argc, char **argv)
{
    std::string filter = "*";
    bool listOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        const char filterArg[] = "--gtest_filter=";
        if (strncmp(argv[i], filterArg, sizeof(filterArg) - 1) == 0)
            filter = argv[i] + sizeof(filterArg) - 1;
        else if (strcmp(argv[i], "--gtest_list_tests") == 0)
            listOnly = true;
    }

    std::vector<const TestInfo *> selected;
    for (const auto &info : registry())
        if (matchesFilter(filter, std::string(info.suiteName) + "." + info.testName))
            selected.push_back(&info);

    if (listOnly)
    {
        const char *lastSuite = "";
        for (auto *info : selected)
        {
            if (strcmp(lastSuite, info->suiteName) != 0)
                printf("%s.\n", lastSuite = info->suiteName);
            printf("  %s\n", info->testName);
        }
        return 0;
    }

    printf("[==========] Running %zu tests.\n", selected.size());
    std::vector<std::string> failed;
    for (auto *info : selected)
    {
        std::string fullName = std::string(info->suiteName) + "." + info->testName;
        printf("[ RUN      ] %s\n", fullName.c_str());
        // Сбрасываем вывод перед тестом: тесты могут создавать процесс через fork.
        fflush(stdout);
        currentTestFailed = false;
        try
        {
            Test *test = info->factory();
            test->SetUp();
            test->TestBody();
            test->TearDown();
            delete test;
        }
        catch (const std::exception &e)
        {
            reportFailure(__FILE__, __LINE__, std::string("Unexpected exception: ") + e.what(), "");
        }
        catch (...)
        {
            reportFailure(__FILE__, __LINE__, "Unexpected exception of unknown type.", "");
        }
        if (currentTestFailed)
        {
            printf("[  FAILED  ] %s\n", fullName.c_str());
            failed.push_back(fullName);
        }
        else
            printf("[       OK ] %s\n", fullName.c_str());
        fflush(stdout);
    }
    printf("[==========] %zu tests ran.\n", selected.size());
    printf("[  PASSED  ] %zu tests.\n", selected.size() - failed.size());
    if (!failed.empty())
    {
        printf("[  FAILED  ] %zu tests, listed below:\n", failed.size());
        for (auto &name : failed)
            printf("[  FAILED  ] %s\n", name.c_str());
    }
    fflush(stdout);
    return failed.empty() ? 0 : 1;
}

} // пространство имён internal
} // пространство имён testing

int main(int argc, char **argv)
{
    return testing::internal::runAllTests(argc, argv);
}
