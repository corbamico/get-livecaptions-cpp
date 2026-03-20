// Tests for caption text parsing and output formatting logic.
// Mirrors the algorithms from Engine::parse_captions(), Engine::ostream_captions(),
// and Engine::ostream_translate() in src/main.cpp.
//
// Build: cl /std:c++20 /EHsc /nologo /Fe:builds\test_caption_parsing.exe tests\test_caption_parsing.cpp
//   or:  g++ -std=c++20 -o test_caption_parsing tests/test_caption_parsing.cpp

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using ParsedCaptions = std::pair<std::string, std::vector<std::string>>;

// Mirror of Engine::parse_captions() logic (the line-splitting part)
static ParsedCaptions parse_captions_text(const std::string& text)
{
    if (text.empty()) return {};
    std::vector<std::string> lines;
    std::string line;
    std::istringstream iss(text);
    while (std::getline(iss, line))
        lines.emplace_back(std::move(line));
    return {text, lines};
}

// Mirror of Engine::find_first_new_line()
static size_t find_first_new_line(const std::vector<std::string>& lines, std::string_view prebuffer)
{
    std::unordered_set<std::string> seen;
    std::string pb_line;
    std::istringstream pbs{std::string{prebuffer}};
    while (std::getline(pbs, pb_line))
        seen.insert(pb_line);
    for (size_t i = 0; i < lines.size(); ++i)
        if (!seen.count(lines[i])) return i;
    return lines.size();
}

// Mirror of Engine::ostream_captions() — returns written content (excluding timestamp line)
static std::string caption_body(const ParsedCaptions& captions, std::string_view prebuffer)
{
    const auto& [current, lines] = captions;
    if (lines.empty()) return "";
    size_t first = find_first_new_line(lines, prebuffer);
    std::ostringstream os;
    for (size_t i = first; i < lines.size(); ++i)
        os << lines[i] << "\n";
    return os.str();
}

// Mirror of Engine::ostream_translate() combined-text building
static std::string translate_combined(const ParsedCaptions& captions, std::string_view prebuffer)
{
    const auto& [current, lines] = captions;
    if (lines.empty()) return "";
    size_t first = find_first_new_line(lines, prebuffer);
    std::string combined;
    for (size_t i = first; i < lines.size(); ++i) {
        combined.append(lines[i]);
        if (i + 1 < lines.size()) combined.push_back('\n');
    }
    return combined;
}

int main()
{
    // === parse_captions_text ===

    // 1. Empty text returns empty result
    {
        auto [text, lines] = parse_captions_text("");
        assert(text.empty());
        assert(lines.empty());
    }

    // 2. Single line (no newline) produces one element
    {
        auto [text, lines] = parse_captions_text("Hello world");
        assert(lines.size() == 1);
        assert(lines[0] == "Hello world");
    }

    // 3. Two lines separated by \n
    {
        auto [text, lines] = parse_captions_text("Line one\nLine two");
        assert(lines.size() == 2);
        assert(lines[0] == "Line one");
        assert(lines[1] == "Line two");
    }

    // 4. Trailing newline does NOT produce an extra empty element
    //    (std::getline behaviour: "a\nb\n" → ["a","b"])
    {
        auto [text, lines] = parse_captions_text("Line one\nLine two\n");
        assert(lines.size() == 2);
    }

    // 5. Empty line in the middle is preserved
    {
        auto [text, lines] = parse_captions_text("first\n\nthird");
        assert(lines.size() == 3);
        assert(lines[1].empty());
    }

    // 6. text field equals original input
    {
        std::string input = "Hello\nWorld";
        auto [text, lines] = parse_captions_text(input);
        assert(text == input);
    }

    // === caption_body (ostream_captions new-line output) ===

    // 7. All lines new → all lines written
    {
        auto captions = parse_captions_text("Line A\nLine B\nLine C");
        auto body = caption_body(captions, "");
        assert(body == "Line A\nLine B\nLine C\n");
    }

    // 8. First line seen → only from second onwards
    {
        auto captions = parse_captions_text("Line A\nLine B\nLine C");
        auto body = caption_body(captions, "Line A");
        assert(body == "Line B\nLine C\n");
    }

    // 9. All lines already seen → body is empty
    {
        auto captions = parse_captions_text("Hello\nWorld");
        auto body = caption_body(captions, "Hello\nWorld");
        assert(body.empty());
    }

    // 10. Empty captions → body is empty
    {
        ParsedCaptions empty;
        auto body = caption_body(empty, "");
        assert(body.empty());
    }

    // 11. Single new line at end of previously seen block
    {
        auto captions = parse_captions_text("Old line\nNew line");
        auto body = caption_body(captions, "Old line");
        assert(body == "New line\n");
    }

    // === translate_combined (ostream_translate combined text) ===

    // 12. All new lines joined with \n (no trailing \n)
    {
        auto captions = parse_captions_text("Hello\nWorld");
        auto combined = translate_combined(captions, "");
        assert(combined == "Hello\nWorld");
    }

    // 13. Single new line — no \n appended
    {
        auto captions = parse_captions_text("Hello");
        auto combined = translate_combined(captions, "");
        assert(combined == "Hello");
    }

    // 14. First line seen — combined starts from second line
    {
        auto captions = parse_captions_text("Old\nNew A\nNew B");
        auto combined = translate_combined(captions, "Old");
        assert(combined == "New A\nNew B");
    }

    // 15. All lines seen — combined is empty (no translation call needed)
    {
        auto captions = parse_captions_text("A\nB");
        auto combined = translate_combined(captions, "A\nB");
        assert(combined.empty());
    }

    // 16. Empty captions → combined is empty
    {
        ParsedCaptions empty;
        auto combined = translate_combined(empty, "");
        assert(combined.empty());
    }

    std::cout << "All tests passed." << std::endl;
    return 0;
}
