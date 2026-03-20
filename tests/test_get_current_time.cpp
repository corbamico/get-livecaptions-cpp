// Tests for get_current_time() format.
// Build: cl /std:c++20 /EHsc /nologo /Fe:builds\test_get_current_time.exe tests\test_get_current_time.cpp
//   or:  g++ -std=c++20 -o test_get_current_time tests/test_get_current_time.cpp

#include <cassert>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>

// Mirror of get_current_time() from src/main.cpp
static std::string get_current_time()
{
    thread_local static std::string time_str;
    thread_local static char buf[64];

    std::time_t now_c = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif

    if (std::strftime(buf, sizeof(buf), "%Y/%m/%d %X", &tm_buf)) {
        time_str.assign(buf);
        return time_str;
    }
    time_str.clear();
    return time_str;
}

int main()
{
    // 1. Returns a non-empty string
    {
        auto t = get_current_time();
        assert(!t.empty());
    }

    // 2. Length is exactly 19 characters: "YYYY/MM/DD HH:MM:SS"
    {
        auto t = get_current_time();
        assert(t.size() == 19);
    }

    // 3. Fixed separator positions: '/' at [4] and [7], ' ' at [10], ':' at [13] and [16]
    {
        auto t = get_current_time();
        assert(t[4]  == '/');
        assert(t[7]  == '/');
        assert(t[10] == ' ');
        assert(t[13] == ':');
        assert(t[16] == ':');
    }

    // 4. All non-separator characters are ASCII digits
    {
        auto t = get_current_time();
        for (size_t i = 0; i < t.size(); ++i) {
            if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
            assert(std::isdigit(static_cast<unsigned char>(t[i])));
        }
    }

    // 5. Matches full regex pattern YYYY/MM/DD HH:MM:SS
    {
        auto t = get_current_time();
        std::regex pattern(R"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2})");
        assert(std::regex_match(t, pattern));
    }

    // 6. Year is plausible (>= 2024)
    {
        auto t = get_current_time();
        int year = std::stoi(t.substr(0, 4));
        assert(year >= 2024);
    }

    // 7. Month is 01–12
    {
        auto t = get_current_time();
        int month = std::stoi(t.substr(5, 2));
        assert(month >= 1 && month <= 12);
    }

    // 8. Day is 01–31
    {
        auto t = get_current_time();
        int day = std::stoi(t.substr(8, 2));
        assert(day >= 1 && day <= 31);
    }

    // 9. Hour is 00–23
    {
        auto t = get_current_time();
        int hour = std::stoi(t.substr(11, 2));
        assert(hour >= 0 && hour <= 23);
    }

    // 10. Minute and second are 00–59
    {
        auto t = get_current_time();
        int minute = std::stoi(t.substr(14, 2));
        int second = std::stoi(t.substr(17, 2));
        assert(minute >= 0 && minute <= 59);
        assert(second >= 0 && second <= 59);
    }

    // 11. Two consecutive calls return same-length strings with matching date portion
    //     (robust: both calls happen within 1 second)
    {
        auto t1 = get_current_time();
        auto t2 = get_current_time();
        assert(t1.size() == t2.size());
        // Date part (first 10 chars) must match — same second
        assert(t1.substr(0, 10) == t2.substr(0, 10));
    }

    std::cout << "All tests passed." << std::endl;
    return 0;
}
