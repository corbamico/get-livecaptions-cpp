#include "pch.h"
#include "LibreTranslate.h"


using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::UIAutomation;

#define VERSION_STRING "0.3.260320"

std::string get_current_time()
{
    // Use thread_local static buffers to avoid repeated allocations.
    // The buffer and string are allocated once per thread and reused on subsequent calls.
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

// Parsed captions: (full text, lines)
using ParsedCaptions = std::pair<std::string, std::vector<std::string>>;

class Engine
{
    winrt::com_ptr<IUIAutomation> _automation;
    winrt::com_ptr<IUIAutomationCondition> _condition;
    winrt::com_ptr<IUIAutomationElement> _cached_text_element; // Cache text_element to reduce repeated lookups
    std::string _prebuffer;
    std::string _prebuffer_translated;
    std::string _sfilename;
    LibreTranslateAPI _ltapi;

    std::optional<std::ofstream> _output_file; // RAII-managed output file
    std::mutex _file_mutex; // protect file operations

    std::atomic<bool> _shutdown_requested{false};  // Add atomic flag

    winrt::hstring get_livecaptions()
    {
        wil::unique_bstr text;
        winrt::com_ptr<IUIAutomationElement> window_element;

        try{
            auto window = FindWindowW(L"LiveCaptionsDesktopWindow", nullptr);
            if (!window) {
                // Window not found, clear cached element
                _cached_text_element = nullptr;
                return winrt::hstring();
            }

            winrt::check_hresult(_automation->ElementFromHandle(window, window_element.put()));

            // Try to use cached text_element first; if it fails, recreate it
            if (_cached_text_element) {
                HRESULT hr = _cached_text_element->get_CurrentName(text.put());
                if (SUCCEEDED(hr) && text) {
                    return text.get();
                }
                // Cached element is stale, clear it and recreate
                _cached_text_element = nullptr;
            }

            // Recreate text_element from window element
            winrt::com_ptr<IUIAutomationElement> text_element;
            winrt::check_hresult(window_element->FindFirst(TreeScope_Descendants, _condition.get(), text_element.put()));
            if (text_element)
            {
                _cached_text_element = text_element; // Cache for next call
                winrt::check_hresult(text_element->get_CurrentName(text.put()));
                return text.get();
            }

            return winrt::hstring();
        }
        catch (winrt::hresult_error &e){
            std::cerr << "[UIAutomation error] " << winrt::to_string(e.message()) << std::endl;
        }
        catch (std::exception &e){
            std::cerr << "[error] " << e.what() << std::endl;
        }
        return winrt::hstring();
    }

    void ostream_translate_line(std::string_view text, std::ostream &os, std::string_view source_lang = "es", std::string_view target_lang = "en")
    {
        try {
            auto tr = _ltapi.translate(std::string{text}, std::string{source_lang}, std::string{target_lang});
            if (tr.is_object() && tr.contains("translatedText")) {
                os << "\033[34m" << tr["translatedText"].get<std::string>() << "\033[0m" << std::endl;
            } else {
                std::cerr << "[translation_error]" << tr.dump() << std::endl;
            }
        } catch (const std::exception &e) {
            std::cerr << "[translation_error] " << e.what() << std::endl;
        } catch (const char *e) {
            std::cerr << "[translation_error] " << e << std::endl;
        }
    }

    void ostream_translate(const ParsedCaptions& captions, std::ostream &os, std::string_view source_lang = "es")
    {
        const auto& [current, lines] = captions;
        if (lines.empty()) return;

        size_t first_new_line = find_first_new_line(lines, _prebuffer_translated);
        _prebuffer_translated = current;

        std::string combined;
        for (size_t i = first_new_line; i < lines.size(); ++i) {
            combined.append(lines[i]);
            if (i + 1 < lines.size()) combined.push_back('\n');
        }

        if (!combined.empty())
            ostream_translate_line(combined, os, source_lang, "en");
    }

    void ostream_captions(const ParsedCaptions& captions, std::ostream &os)
    {
        const auto& [current, lines] = captions;
        if (lines.empty()) return;

        size_t first_new_line = find_first_new_line(lines, _prebuffer);
        _prebuffer = current;

        os << "[" << get_current_time() << "] " << std::endl;
        for (size_t i = first_new_line; i < lines.size(); ++i) {
            os << lines[i] << std::endl;
        }
    }

    // Returns index of first new line by exact-line membership check (avoids false substring matches)
    size_t find_first_new_line(const std::vector<std::string>& lines, std::string_view prebuffer)
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

public:
    Engine(std::string_view filename, std::string_view translate_host="http://127.0.0.1:5000/") : _sfilename{filename}, _ltapi{std::string(translate_host)}
    {
        winrt::init_apartment();
        _automation = try_create_instance<IUIAutomation>(guid_of<CUIAutomation>());
        winrt::check_hresult(_automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, wil::make_variant_bstr(L"CaptionsTextBlock"), _condition.put()));
    }

    // Add proper file closing in destructor
    ~Engine() {
        std::scoped_lock lock(_file_mutex);
        if (_output_file && _output_file->is_open()) {
            _output_file->flush();
            _output_file->close();
            _output_file.reset();
        }
    }

    static bool is_livecaption_running()
    {
        return FindWindowW(L"LiveCaptionsDesktopWindow", nullptr) != NULL;
    }

    ParsedCaptions parse_captions()
    {
        auto hs_current = get_livecaptions();
        if(hs_current.empty()) return {};
        auto current = winrt::to_string(hs_current);

        std::vector<std::string> lines;
        std::string line;
        std::istringstream iss(current);
        while (std::getline(iss, line)) {
            lines.emplace_back(std::move(line));
        }
        return {std::move(current), std::move(lines)};
    }

    void output_translate(const ParsedCaptions& captions, std::string_view source_lang)
    {
        if (!_sfilename.empty() && _sfilename[0] == '-') {
            ostream_translate(captions, std::cout, source_lang);
            return;
        }

        std::scoped_lock lock(_file_mutex);
        if (!_output_file || !_output_file->is_open()) {
            _output_file.emplace(_sfilename, std::ios::app);
        }
        if (_output_file && _output_file->is_open()) {
            ostream_translate(captions, *_output_file, source_lang);
        }
    }

    void output_captions(const ParsedCaptions& captions)
    {
        if(!_sfilename.empty() && _sfilename[0] == '-') {
            ostream_captions(captions, std::cout);
            return;
        }

        // Only open once; use RAII optional to manage file lifetime
        std::scoped_lock lock(_file_mutex);
        if (!_output_file || !_output_file->is_open()) {
            _output_file.emplace(_sfilename, std::ios::app);
        }

        if (_output_file && _output_file->is_open()) {
            ostream_captions(captions, *_output_file);
        }
    }

    bool touch_file()
    {
        if(!_sfilename.empty() && _sfilename[0] == '-'){
            return true;
        }

        std::ofstream file(_sfilename,std::ios::app);
        auto ret = file.is_open();
        file.close();
        return ret;
    }

    bool is_shutdown_requested() const { return _shutdown_requested.load(std::memory_order_acquire); }
    void request_shutdown() { _shutdown_requested.store(true, std::memory_order_release); }

    // Perform full shutdown/cleanup: flush & close file, set flag, uninitialize COM
    void shutdown()
    {
        request_shutdown();
        try {
            std::scoped_lock lock(_file_mutex);
            if (_output_file && _output_file->is_open()) {
                // write any remaining captions before closing
                auto captions = parse_captions();
                ostream_captions(captions, *_output_file);
                _output_file->flush();
                _output_file->close();
                _output_file.reset();
            }
        }
        catch (...) {}

        // Uninitialize apartment if needed
        try {
            winrt::uninit_apartment();
        }
        catch (...) {}
    }
};

int main(int argc, char *argv[])
{
    int exit_code = 0;
    std::string strFileName;
    std::string strTranslateLang;
    std::string strTranslateHost{"http://127.0.0.1:5000/"};
    int nPollInterval = 10;
    int nFlushInterval = 60;
    argparse::ArgumentParser program("get-livecaptions",VERSION_STRING);

    program.add_argument("-t", "--translate")
        .help("translation from language, values as es,fr,de,it.")
        .metavar("<lang>")
        .choices("es", "eu", "fr", "de", "it");

    program.add_argument("--translate-host")
        .help("libretranslate server running at HOST, default as (http://127.0.0.1:5000).")
        .metavar("HOST");

    program.add_argument("-o", "--output")
        .metavar("<file>")
        .help("filename, write content into file. use - for console.")
        .required();

    program.add_argument("--poll-interval")
        .help("UIAutomation polling interval in seconds (default: 10).")
        .metavar("<seconds>")
        .scan<'i', int>();

    program.add_argument("--flush-interval")
        .help("output file flush interval in seconds (default: 60).")
        .metavar("<seconds>")
        .scan<'i', int>();

    program.add_description("Write the content of LiveCaptions Windows System Program into file, continually.\nTranslate captions if --translate is specified, using libretranslate(gh:LibreTranslate/LibreTranslate).");
    program.add_epilog("use ctrl-c to exit program.");

    try {
        if(argc==1) {program.print_help();exit(1);}
        program.parse_args(argc, argv);
        strFileName = program.get<std::string>("--output");

        if (program.is_used("--translate")){
            strTranslateLang = program.get<std::string>("--translate");
            if (program.is_used("--translate-host"))
                strTranslateHost = program.get<std::string>("--translate-host");
        }

        nPollInterval = program.is_used("--poll-interval")
            ? program.get<int>("--poll-interval") : 10;
        nFlushInterval = program.is_used("--flush-interval")
            ? program.get<int>("--flush-interval") : 60;

        if (nPollInterval < 1) {
            std::cerr << "--poll-interval must be >= 1" << std::endl;
            return 1;
        }
        if (nFlushInterval < 1) {
            std::cerr << "--flush-interval must be >= 1" << std::endl;
            return 1;
        }

        if (!Engine::is_livecaption_running())
        {
            std::cerr << "[Error]Live Captions is not running." <<std::endl;
            exit(1);
        }
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    try
    {
        asio::io_context io_context(1);

        // Use a shared_ptr to capture by value in the lambda
        auto engine_ptr=std::make_unique<Engine>(strFileName,strTranslateHost);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){
                                engine_ptr->shutdown();
                                io_context.stop();
                            });
        std::cout << "Save content into file, every " << nFlushInterval << " seconds." << std::endl;
        asio::co_spawn(io_context, [&]() -> asio::awaitable<void>
                       {
                            asio::steady_timer timer_poll(io_context);
                            asio::steady_timer timer_flush(io_context);

                            // Initialize flush timer
                            timer_flush.expires_after(asio::chrono::seconds(nFlushInterval));

                            while(!engine_ptr->is_shutdown_requested()){
                                timer_poll.expires_after(asio::chrono::seconds(nPollInterval));
                                co_await timer_poll.async_wait(asio::use_awaitable);

                                if (!Engine::is_livecaption_running())
                                {
                                    std::cerr << "[Info]LiveCaptions isn't running. exit." <<std::endl;
                                    engine_ptr->shutdown();
                                    io_context.stop();
                                    break;
                                }

                                bool need_translate = !strTranslateLang.empty();
                                bool need_flush = std::chrono::steady_clock::now() >= timer_flush.expiry();

                                // Single UIAutomation call per tick, only when a consumer is due
                                if (need_translate || need_flush) {
                                    auto captions = engine_ptr->parse_captions();

                                    if (need_translate)
                                        engine_ptr->output_translate(captions, strTranslateLang);

                                    if (need_flush) {
                                        engine_ptr->output_captions(captions);
                                        timer_flush.expires_after(asio::chrono::seconds(nFlushInterval));
                                    }
                                }
                            }
                            co_return;
                        },
                        asio::detached);
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::printf("Exception: %s\n", e.what());
        exit_code = 1;
    }
    return exit_code;
}
