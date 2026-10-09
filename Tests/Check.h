// A test runner small enough to read in one go. No JUCE, no framework.
#pragma once

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace check
{
struct Test { const char* name; std::function<void()> fn; };
inline std::vector<Test>& all() { static std::vector<Test> t; return t; }
inline int& failures() { static int f = 0; return f; }
inline const char*& current() { static const char* c = ""; return c; }

struct Register { Register (const char* name, std::function<void()> fn) { all().push_back ({ name, std::move (fn) }); } };

inline void fail (const char* file, int line, const std::string& what)
{
    std::printf ("FAIL %s (%s:%d): %s\n", current(), file, line, what.c_str());
    ++failures();
}

template <typename A, typename B>
std::string describe (const A& a, const B& b)
{
    std::ostringstream s;
    s << "got " << a << ", wanted " << b;
    return s.str();
}
} // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2 (a, b)
#define TEST(name) \
    static void CHECK_CAT (test_, __LINE__)(); \
    static check::Register CHECK_CAT (reg_, __LINE__) (name, CHECK_CAT (test_, __LINE__)); \
    static void CHECK_CAT (test_, __LINE__)()
#define CHECK(cond) do { if (! (cond)) check::fail (__FILE__, __LINE__, #cond); } while (0)
#define CHECK_EQ(a, b) do { const auto& va_ = (a); const auto& vb_ = (b); \
    if (! (va_ == vb_)) check::fail (__FILE__, __LINE__, std::string (#a " == " #b ": ") + check::describe (va_, vb_)); } while (0)
