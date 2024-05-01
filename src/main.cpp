#include "pch.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::UIAutomation;

std::string get_current_time()
{
    // [TIPS]c+20 runtime is too expensive, + 500k for following implement.
    //
    // auto const time = std::chrono::current_zone()
    //     ->to_local(std::chrono::system_clock::now());
    // return std::format("{:%Y/%m/%d %X}", time);

    auto now = std::chrono::system_clock::now();
    std::stringstream ss;
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);    
    ss<<std::put_time(std::localtime(&now_c), "%Y/%m/%d %X");
    return ss.str();
}

class Engine
{
    winrt::com_ptr<IUIAutomation> _automation;
    winrt::com_ptr<IUIAutomationCondition> _condition;
    std::string _prebuffer;
    std::string _sfilename;

    winrt::hstring get_livecaptions()
    {
        wil::unique_bstr text;
        winrt::com_ptr<IUIAutomationElement> window_element;
        winrt::com_ptr<IUIAutomationElement> text_element;

        try{
            auto window = FindWindowW(L"LiveCaptionsDesktopWindow", nullptr);
            winrt::check_hresult(_automation->ElementFromHandle(window, window_element.put()));
            winrt::check_hresult(window_element->FindFirst(TreeScope_Descendants, _condition.get(), text_element.put()));
            if (text_element)
            {
                winrt::check_hresult(text_element->get_CurrentName(text.put()));
                return text.get();
            }
            return winrt::hstring();
        }
        catch (winrt::hresult_error &e){}
        catch (std::exception &e){}
        return winrt::hstring();
    }

public:
    Engine(const std::string &filename) : _sfilename{filename}
    {
        winrt::init_apartment();
        _automation = try_create_instance<IUIAutomation>(guid_of<CUIAutomation>());
        winrt::check_hresult(_automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, wil::make_variant_bstr(L"CaptionsTextBlock"), _condition.put()));
    }
    ~Engine() { winrt::uninit_apartment(); }

    void save_current_captions(bool includeLastLine = false)
    {
        auto hs_current = get_livecaptions();
        if(hs_current.empty()) return;
        auto current = winrt::to_string(hs_current);
        
        std::vector<std::string> lines;
        std::string line;
        std::istringstream iss(current);
        while (std::getline(iss, line))
        {
            lines.emplace_back(line);
        }
        // Find the first line not in the prebuffer
        size_t first_new_line = lines.size();
        for (size_t i = 0; i < lines.size(); i++)
        {
            if (_prebuffer.find(lines[i]) == std::string::npos)
            {
                first_new_line = i;
                break;
            }
        }
        _prebuffer = current;

        if (first_new_line < lines.size())
        {
            // Append new lines to the file and prebuffer
            std::ofstream file(_sfilename, std::ios::app);
            if (!file.is_open())
            {
                std::cerr << "[Error]Failed to open file: " << _sfilename << std::endl;
                return;
            }


            file << "[" << get_current_time() << "] " << std::endl;

            for (size_t i = first_new_line; i < lines.size(); ++i)
            {
                file << lines[i] << std::endl;
            }
            file.flush();
            file.close();
        }
    }
    static bool is_livecaption_running()
    {
        return FindWindowW(L"LiveCaptionsDesktopWindow", nullptr) != NULL;
    }
};



void usage()
{
    std::cerr << "Write all content of LiveCaptions Windows System Program into file, continually.Ctrl-C to exit." << std::endl;
    std::cerr << "Usage: get-livecatpions file" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  file            filename, to save content of live catpions running." << std::endl;
    exit(1);
}
bool touch_file(const std::string &filename)
{
    std::ofstream file(filename,std::ios::app);
    auto ret = file.is_open();
    file.close();
    return ret;
}

int main(int argc, char *argv[])
{
    if ((argc != 2) || (!touch_file(argv[1])))
        usage();
    if (!Engine::is_livecaption_running())
    {
        std::cerr << "[Error]Live Captions is not running." <<std::endl; 
        exit(1);
    }

    try
    {
        asio::io_context io_context(1);
        Engine eng(argv[1]);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ 
                                std::cerr << "ctrl-c to exit." <<std::endl;        
                                eng.save_current_captions(); 
                                io_context.stop();
                            });
        std::cout<<"Save content into file, every 1 min."<<std::endl;
        asio::co_spawn(io_context, [&]() -> asio::awaitable<void>
                       {
                            asio::steady_timer timer_10s(io_context);
                            ULONG64 ulCount{0};
                            while(true){            
                                timer_10s.expires_after(asio::chrono::seconds(10));
                                co_await timer_10s.async_wait(asio::use_awaitable);
                                //std::cout << "every 10s" <<std::endl;
                                if (!Engine::is_livecaption_running())
                                {
                                    std::cerr << "[Info]LiveCaptions isn't running. exit." <<std::endl;
                                    io_context.stop();
                                    break;
                                }
                                if(ulCount++%6==0) eng.save_current_captions();            
                            }
                            co_return; 
                        }, 
                        asio::detached);
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::printf("Exception: %s\n", e.what());
    }
    return 0;
}