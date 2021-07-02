#include "stdx.h"

#include <chrono>
#include <ostream>
#include <iomanip>

namespace stdx{
    //the global function that stdx will take to write logs
    //if g_stdx_log_fn is set by set_stdx_log_func,stdx will output log 
    std::function<void(int level,const std::string& msg)> g_stdx_log_fn;

    void set_stdx_log_func(std::function<void(int level,const std::string& msg)> fn) 
    {
        g_stdx_log_fn = fn;
    }

    std::function<void(int level,const std::string& msg)> get_stdx_log_function()
    {
        return g_stdx_log_fn;
    } 


    std::string time_str()
    {

        auto tp_now = std::chrono::system_clock::now();
    
        time_t t = std::chrono::system_clock::to_time_t(tp_now);
        struct tm* tm = std::localtime(&t);
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp_now.time_since_epoch()).count() - 1000*t;
    
        std::ostringstream oss;
        oss << std::put_time(tm,"%Y-%m-%d %H:%M:%S") << "." << ms;

        return oss.str();

    }

    std::string file_name(const std::string& path) 
    {
        auto pos = path.find_last_of('/');
        if(pos != std::string::npos)
        {
            return path.substr(pos+1);
        }

        pos = path.find_last_of('\\');
        if(pos != std::string::npos)
        {
            return path.substr(pos+1);
        }

        return path;
    }
    
}
