#include "pch.h"
#include "LibreTranslate.h"


using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::UIAutomation;

#define VERSION_STRING "0.2.251225"

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

class Engine
{
    winrt::com_ptr<IUIAutomation> _automation;
    winrt::com_ptr<IUIAutomationCondition> _condition;
    winrt::com_ptr<IUIAutomationElement> _cached_text_element; // Cache text_element to reduce repeated lookups
    std::string _prebuffer;
    std::string _prebuffer_tanslated;
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
        catch (winrt::hresult_error &e){}
        catch (std::exception &e){}
        return winrt::hstring();
    }
    void ostream_translate_line(std::string_view text, std::ostream &os = std::cerr, std::string_view source_lang = "es", std::string_view target_lang = "en")
    {

        try {
            auto tr = _ltapi.translate(std::string{text}, std::string{source_lang}, std::string{target_lang});
            if (tr.is_object() && tr.contains("translatedText")) {
                os << "\033[34m" << tr["translatedText"].get<std::string>() << "\033[0m" << std::endl;
            } else {
                os << "[translation_error]" << tr.dump() << std::endl;
            }
        } catch (const std::exception &e) {
            os << "[translation_error] " << e.what() << std::endl;
        } catch (const char *e) {
            os << "[translation_error] " << e << std::endl;
        }
    }

    void ostream_translate(std::ostream &os = std::cerr, std::string_view source_lang = "es")
    {
        auto [current, lines] = parse_captions();
        if (lines.empty()) return;

        size_t first_new_line = find_first_new_line(lines, _prebuffer_tanslated);
        _prebuffer_tanslated = std::move(current);

        std::string combined;
        for (size_t i = first_new_line; i < lines.size(); ++i) {
            combined.append(lines[i]);
            if (i + 1 < lines.size()) combined.push_back('\n');
        }

        if (!combined.empty()) 
            ostream_translate_line(combined, os, source_lang, "en");
    }

    void ostream_captions(std::ostream &os)
    {
        auto [current, lines] = parse_captions();
        if (lines.empty()) return;

        size_t first_new_line = find_first_new_line(lines, _prebuffer);
        _prebuffer = std::move(current);

        os << "[" << get_current_time() << "] " << std::endl;
        for (size_t i = first_new_line; i < lines.size(); ++i) {
            os << lines[i] << std::endl;
        }
    }

    // New helper method to reduce duplication
    std::pair<std::string, std::vector<std::string>> parse_captions()
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

    // Returns index of first new line, or lines.size() if all processed
    size_t find_first_new_line(const std::vector<std::string>& lines, std::string_view prebuffer)
    {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (prebuffer.find(lines[i]) == std::string_view::npos) {
                return i;
            }
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

    void output_translate(std::string_view source_lang)
    {
        ostream_translate(std::cerr, source_lang);
    }
    void output_captions()
    {
        if(!_sfilename.empty() && _sfilename[0] == '-') {
            ostream_captions(std::cout);
            return;
        }

        // Only open once; use RAII optional to manage file lifetime
        std::scoped_lock lock(_file_mutex);
        if (!_output_file || !_output_file->is_open()) {
            _output_file.emplace(_sfilename, std::ios::app);
        }

        if (_output_file && _output_file->is_open()) {
            ostream_captions(*_output_file);
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
                ostream_captions(*_output_file);
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
                                //std::cerr << "ctrl-c to exit." <<std::endl;
                                engine_ptr->shutdown();
                                io_context.stop();
                            });
        std::cout<<"Save content into file, every 1 min."<<std::endl;
        asio::co_spawn(io_context, [&]() -> asio::awaitable<void>
                       {
                            asio::steady_timer timer_10s(io_context);
                            asio::steady_timer timer_60s(io_context);
                            
                            // Initialize timer_60s to expire after 60 seconds
                            timer_60s.expires_after(asio::chrono::seconds(60));
                            
                            while(!engine_ptr->is_shutdown_requested()){
                                timer_10s.expires_after(asio::chrono::seconds(10));
                                co_await timer_10s.async_wait(asio::use_awaitable);

                                if (!Engine::is_livecaption_running())
                                {
                                    std::cerr << "[Info]LiveCaptions isn't running. exit." <<std::endl;
                                    engine_ptr->shutdown();
                                    io_context.stop();
                                    break;
                                }

                                // Output translation if language is specified, every 10s
                                if(!strTranslateLang.empty()) 
                                    engine_ptr->output_translate(strTranslateLang);

                                // Output captions every 60s
                                if(std::chrono::steady_clock::now() >= timer_60s.expiry()) {
                                    engine_ptr->output_captions();
                                    timer_60s.expires_after(asio::chrono::seconds(60));
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
