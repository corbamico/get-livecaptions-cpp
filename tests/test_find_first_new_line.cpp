// Standalone assertion-based tests for the find_first_new_line logic.
// Build: cl /std:c++20 /EHsc test_find_first_new_line.cpp
//   or:  g++ -std=c++20 -o test_find_first_new_line test_find_first_new_line.cpp
// No test framework required.

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// Mirror of the function from src/main.cpp
static size_t find_first_new_line(const std::vector<std::string>& lines, std::string_view prebuffer)
{
    std::unordered_set<std::string> seen;
    std::string pb_line;
    std::istringstream pbs{std::string{prebuffer}};
    while (std::getline(pbs, pb_line))
        seen.insert(pb_line);

    for (size_t i = 0; i < lines.size(); ++i) {
        if (!seen.count(lines[i]))
            return i;
    }
    return lines.size();
}

int main()
{
    // 1. Empty prebuffer — all lines are new; first new line is index 0
    {
        std::vector<std::string> lines = {"Hello", "world"};
        assert(find_first_new_line(lines, "") == 0);
    }

    // 2. All lines already seen — returns lines.size()
    {
        std::vector<std::string> lines = {"Hello", "world"};
        assert(find_first_new_line(lines, "Hello\nworld") == lines.size());
    }

    // 3. Some lines seen, some new — returns index of first new
    {
        std::vector<std::string> lines = {"Hello", "world", "new line"};
        assert(find_first_new_line(lines, "Hello\nworld") == 2);
    }

    // 4. FALSE-MATCH regression: short line "is" must NOT match because
    //    "This is a test" contains "is" as a substring.
    //    With the old prebuffer.find() approach this would have returned lines.size()
    //    (wrongly treating "is" as already seen). The set-based fix returns 0.
    {
        std::vector<std::string> lines = {"is"};
        assert(find_first_new_line(lines, "This is a test") == 0);
    }

    // 5. FALSE-MATCH regression: "Hello" must not match "Hello world"
    {
        std::vector<std::string> lines = {"Hello"};
        assert(find_first_new_line(lines, "Hello world") == 0);
    }

    // 6. Exact match works correctly — "Hello world" seen, "Hello world" new should be seen
    {
        std::vector<std::string> lines = {"Hello world"};
        assert(find_first_new_line(lines, "Hello world") == lines.size());
    }

    // 7. Empty lines vector — returns 0 (lines.size() == 0)
    {
        std::vector<std::string> lines;
        assert(find_first_new_line(lines, "some previous content") == 0);
    }

    // 8. First line seen, second line new
    {
        std::vector<std::string> lines = {"seen line", "new line"};
        assert(find_first_new_line(lines, "seen line") == 1);
    }

    std::cout << "All tests passed." << std::endl;
    return 0;
}
