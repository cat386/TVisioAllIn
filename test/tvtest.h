#ifndef TVISION_TVTEST_H
#define TVISION_TVTEST_H

// Минимальная среда модульного тестирования, совместимая с используемой
// в Turbo Vision частью GoogleTest: TEST(Suite, Name), TEST_F(Fixture,
// Name), фикстуры от testing::Test с конструктором, деструктором и
// SetUp/TearDown. Поддерживает EXPECT_EQ, EXPECT_NE, EXPECT_TRUE,
// EXPECT_FALSE и варианты ASSERT_*, с добавлением сообщения через '<<';
// testing::PrintToString; параметры --gtest_filter=POSITIVE[-NEGATIVE] и
// --gtest_list_tests. Позволяет собирать тесты без внешних зависимостей.
//
//
//
//
//

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstddef>

namespace testing
{

class Message
{
    std::ostringstream ss;

public:

    Message() = default;
    Message(const Message &other) { ss << other.str(); }

    template <class T>
    Message &operator<<(const T &value);
    Message &operator<<(std::ostream &(*manip)(std::ostream &))
    {
        ss << manip;
        return *this;
    }

    std::string str() const { return ss.str(); }
};

class Test
{
public:

    virtual ~Test() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
    virtual void TestBody() = 0;
};

class AssertionResult
{
    bool success;
    std::string msg;

public:

    AssertionResult(bool aSuccess, std::string aMsg = {}) :
        success(aSuccess),
        msg(std::move(aMsg))
    {
    }

    explicit operator bool() const { return success; }
    const std::string &message() const { return msg; }
};

namespace internal
{

// Вывод значений.

template <class T, class = void>
struct IsStreamable : std::false_type {};

template <class T>
struct IsStreamable<T, decltype(void(std::declval<std::ostream &>() << std::declval<const T &>()))> : std::true_type {};

template <class T, class = void>
struct IsContainer : std::false_type {};

template <class T>
struct IsContainer<T, decltype(void(std::declval<const T &>().begin() != std::declval<const T &>().end()))> : std::true_type {};

void printBytes(const unsigned char *data, size_t size, std::ostream &os);
void printChar(int ch, std::ostream &os);
void printCharArray(const char *data, size_t size, std::ostream &os);

template <class T>
void printValue(const T &value, std::ostream &os);
inline void printValue(const bool &value, std::ostream &os);
inline void printValue(const char &value, std::ostream &os);
inline void printValue(const signed char &value, std::ostream &os);
inline void printValue(const unsigned char &value, std::ostream &os);
inline void printValue(const std::nullptr_t &, std::ostream &os);
template <size_t N>
inline void printValue(const char (&value)[N], std::ostream &os);
template <class T, size_t N>
inline void printValue(const T (&value)[N], std::ostream &os);

template <class T>
inline void printDispatch(const T &value, std::ostream &os, std::true_type /* Сериализуемый в поток. */, ...)
{
    os << value;
}

template <class T>
inline void printDispatch(const T &value, std::ostream &os, std::false_type, std::true_type /* Контейнер. */)
{
    os << "{ ";
    bool first = true;
    for (const auto &item : value)
    {
        if (!first)
            os << ", ";
        first = false;
        printValue(item, os);
    }
    os << (first ? "}" : " }");
}

template <class T>
inline void printDispatch(const T &value, std::ostream &os, std::false_type, std::false_type)
{
    printBytes((const unsigned char *) &value, sizeof(value), os);
}

template <class T>
inline void printValue(const T &value, std::ostream &os)
{
    printDispatch(value, os, IsStreamable<T>(), IsContainer<T>());
}

inline void printValue(const bool &value, std::ostream &os) { os << (value ? "true" : "false"); }
inline void printValue(const char &value, std::ostream &os) { printChar((unsigned char) value, os); }
inline void printValue(const signed char &value, std::ostream &os) { printChar((unsigned char) value, os); }
inline void printValue(const unsigned char &value, std::ostream &os) { printChar(value, os); }
inline void printValue(const std::nullptr_t &, std::ostream &os) { os << "(nullptr)"; }

template <size_t N>
inline void printValue(const char (&value)[N], std::ostream &os) { printCharArray(value, N, os); }

template <class T, size_t N>
inline void printValue(const T (&value)[N], std::ostream &os)
{
    os << "{ ";
    for (size_t i = 0; i < N; ++i)
    {
        if (i > 0)
            os << ", ";
        printValue(value[i], os);
    }
    os << " }";
}

// Регистрация и выполнение тестов.

struct TestInfo
{
    const char *suiteName;
    const char *testName;
    Test *(*factory)();
};

std::vector<TestInfo> &registry();
void reportFailure(const char *file, int line, const std::string &summary, const std::string &message);

struct Registrar
{
    Registrar(const char *suiteName, const char *testName, Test *(*factory)())
    {
        registry().push_back({suiteName, testName, factory});
    }
};

class AssertHelper
{
    const char *file;
    int line;
    const std::string &summary;

public:

    AssertHelper(const char *aFile, int aLine, const std::string &aSummary) :
        file(aFile), line(aLine), summary(aSummary)
    {
    }

    void operator=(const Message &message) const
    {
        reportFailure(file, line, summary, message.str());
    }
};

// Тесты часто сравнивают значения разной знаковости. Как GoogleTest, чьи
// заголовки обычно считаются системными, не выдаём предупреждений об
// этом.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

template <class T1, class T2>
AssertionResult cmpFailure( const char *op, const char *expr1, const char *expr2,
                            const T1 &val1, const T2 &val2 )
{
    std::ostringstream os;
    os << "Expected: (" << expr1 << ") " << op << " (" << expr2 << "), actual: ";
    printValue(val1, os);
    os << " vs ";
    printValue(val2, os);
    return AssertionResult(false, os.str());
}

template <class T1, class T2>
AssertionResult cmpHelperEQ(const char *expr1, const char *expr2, const T1 &val1, const T2 &val2)
{
    if (val1 == val2)
        return AssertionResult(true);
    std::ostringstream os;
    os << "Expected equality of these values:\n  " << expr1 << "\n    Which is: ";
    printValue(val1, os);
    os << "\n  " << expr2 << "\n    Which is: ";
    printValue(val2, os);
    return AssertionResult(false, os.str());
}

template <class T1, class T2>
AssertionResult cmpHelperNE(const char *expr1, const char *expr2, const T1 &val1, const T2 &val2)
{
    if (val1 != val2)
        return AssertionResult(true);
    return cmpFailure("!=", expr1, expr2, val1, val2);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

inline AssertionResult boolHelper(const char *expr, bool value, bool expected)
{
    if (value == expected)
        return AssertionResult(true);
    std::ostringstream os;
    os << "Value of: " << expr << "\n  Actual: " << (value ? "true" : "false")
       << "\nExpected: " << (expected ? "true" : "false");
    return AssertionResult(false, os.str());
}

int runAllTests(int argc, char **argv);

} // пространство имён internal

namespace internal
{

template <class T>
inline void streamValue(const T &value, std::ostream &os, std::true_type /* Сериализуемый в поток. */)
{
    os << value;
}

template <class T>
inline void streamValue(const T &value, std::ostream &os, std::false_type)
{
    printValue(value, os);
}

} // пространство имён internal

template <class T>
inline Message &Message::operator<<(const T &value)
{
    // Как в GoogleTest, значения с оператором вывода в поток записываются без преобразований.
    internal::streamValue(value, ss, internal::IsStreamable<T>());
    return *this;
}

template <class T>
inline std::string PrintToString(const T &value)
{
    std::ostringstream os;
    internal::printValue(value, os);
    return os.str();
}

} // пространство имён testing

#define TVTEST_TEST_CLASS_NAME_(suite, name) suite##_##name##_Test

#define TVTEST_DEFINE_TEST_(suite, name, parent)                                   \
    class TVTEST_TEST_CLASS_NAME_(suite, name) : public parent                     \
    {                                                                               \
    public:                                                                         \
        void TestBody() override;                                                   \
        static ::testing::Test *create() { return new TVTEST_TEST_CLASS_NAME_(suite, name); } \
    };                                                                              \
    static ::testing::internal::Registrar suite##_##name##_registrar_(              \
        #suite, #name, &TVTEST_TEST_CLASS_NAME_(suite, name)::create);              \
    void TVTEST_TEST_CLASS_NAME_(suite, name)::TestBody()

#define TEST(suite, name) TVTEST_DEFINE_TEST_(suite, name, ::testing::Test)
#define TEST_F(fixture, name) TVTEST_DEFINE_TEST_(fixture, name, fixture)

#define TVTEST_CHECK_(result, onFailure)                                           \
    switch (0) case 0: default:                                                     \
    if (const ::testing::AssertionResult tvtest_ar_ = (result))                     \
        ;                                                                           \
    else                                                                            \
        onFailure ::testing::internal::AssertHelper(__FILE__, __LINE__, tvtest_ar_.message()) = ::testing::Message()

#define TVTEST_NONFATAL_
#define TVTEST_FATAL_ return

#define EXPECT_EQ(a, b) TVTEST_CHECK_(::testing::internal::cmpHelperEQ(#a, #b, a, b), TVTEST_NONFATAL_)
#define EXPECT_NE(a, b) TVTEST_CHECK_(::testing::internal::cmpHelperNE(#a, #b, a, b), TVTEST_NONFATAL_)
#define EXPECT_TRUE(a) TVTEST_CHECK_(::testing::internal::boolHelper(#a, bool(a), true), TVTEST_NONFATAL_)
#define EXPECT_FALSE(a) TVTEST_CHECK_(::testing::internal::boolHelper(#a, bool(a), false), TVTEST_NONFATAL_)
#define ASSERT_EQ(a, b) TVTEST_CHECK_(::testing::internal::cmpHelperEQ(#a, #b, a, b), TVTEST_FATAL_)
#define ASSERT_NE(a, b) TVTEST_CHECK_(::testing::internal::cmpHelperNE(#a, #b, a, b), TVTEST_FATAL_)
#define ASSERT_TRUE(a) TVTEST_CHECK_(::testing::internal::boolHelper(#a, bool(a), true), TVTEST_FATAL_)
#define ASSERT_FALSE(a) TVTEST_CHECK_(::testing::internal::boolHelper(#a, bool(a), false), TVTEST_FATAL_)

#endif // TVISION_TVTEST_H
