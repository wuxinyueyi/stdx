#ifndef __STDX_LOGGER_LOG_H__
#define __STDX_LOGGER_LOG_H__


#include <stdint.h>
#include <thread>
#include <stdarg.h>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <fstream>
#include <map>

namespace stdx{namespace thread{
    class TaskThreadPool;
}}


namespace stdx{namespace log{

std::string get_file_name(const std::string& path);


enum ELogLevel
{
    info = 0x1,
    debug = 0x2,
    warn = 0x4,
    error = 0x8,
    fatal = 0x10,
};

enum ELogOutType
{
    file = 0x1,
    console = 0x2,
};


class Logger{
public:
    virtual ~Logger() = default;
    static Logger& instance();
       
    void start(const std::string& log_file,int level_mask=0xF,int out_mask=0xF,int max_size=256,bool async=true);
    void stop();

    inline void set_level_mask(int level){log_level_mask_ = level;};
    inline void set_out_type_mask(int type){out_put_type_mask_ = type;};
    inline void set_max_file_size(int size){max_size_m_ = size;};

    void format_log(int level,const char* file,uint32_t line,const char* format,...);

    void raw_log(int level,const std::string& msg);
    
private:
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    void log(const std::string& str,int level);
    std::string this_thread_id_str();
    std::string get_bak_file_name();   

private:    
    static std::unique_ptr<Logger> instance_;
    static std::mutex mtx_instance_;

    int log_level_mask_;
    int out_put_type_mask_;

    int max_size_m_;

    std::string file_path_;
    std::ofstream ofs_;
    bool async_;

    std::atomic<bool> startd_;

    std::map<int,std::string> level_map_;

    std::shared_ptr<stdx::thread::TaskThreadPool> write_task_thread_;

    std::mutex wrt_mutex_;

    std::map<std::thread::id,std::vector<char>> buffers1_;
    std::map<std::thread::id,std::vector<char>> buffers2_;

    std::mutex buffer_mutex_;
};






}}

#endif