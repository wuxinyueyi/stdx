#include "log/log.h"
#include  "thread/task_thread_pool.h"

#include <vector>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace stdx{namespace log{
      
std::unique_ptr<Logger> Logger::instance_;
std::mutex Logger::mtx_instance_;

Logger::Logger():log_level_mask_(0xF),out_put_type_mask_(0xF),max_size_m_(256),async_(false),startd_(false)
{
    level_map_[ELogLevel::info] = "Info";
    level_map_[ELogLevel::debug] = "Debug";
    level_map_[ELogLevel::warn] = "Warn";
    level_map_[ELogLevel::error] = "Error";
    level_map_[ELogLevel::fatal] = "Fatal";
}

Logger& Logger::instance() 
{    
    if(nullptr == instance_)
    {        
        while(!mtx_instance_.try_lock())
        {
           std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }    

        if(nullptr == instance_)
        {
            instance_.reset(new Logger());
        }
        
        mtx_instance_.unlock();    
    }

    return *instance_.get();
}

void Logger::start(const std::string& log_file,int level_mask,int out_mask,int max_size,bool async) 
{
    if(startd_)
    {
        return;
    }

    file_path_ = log_file;
    log_level_mask_ = level_mask;
    out_put_type_mask_ = out_mask;
    max_size_m_ = max_size;
    async_ = async;

    if(!file_path_.empty() && (out_mask&ELogOutType::file))
    {
        ofs_ = std::ofstream(file_path_,std::ios_base::out|std::ios_base::app);
    }

    if(async_)
    {
        write_task_thread_ = std::make_shared<stdx::thread::TaskThreadPool>();
        write_task_thread_->set_name("Logger thread pool");
        write_task_thread_->set_task_timeout(0);//do not log self
        if(!write_task_thread_->start(1,1))
        {
            async_ = false;
        }
    }

    startd_ = true;
}

void Logger::stop() 
{
    startd_ = false;

    while (async_ 
    && write_task_thread_ 
    && write_task_thread_->task_count() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    

    ofs_.flush();
    ofs_.close();        
}

void Logger::log(const std::string& str,int level) 
{   
    if(!startd_) 
    {
        return;
    }

    if(out_put_type_mask_&ELogOutType::console)
    {
        if(ELogLevel::error&level
        || ELogLevel::fatal&level)
        {
            std::cerr << str << std::endl;
        }else
        {
            std::cout << str << std::endl;
        }
    }
    
    if(out_put_type_mask_&ELogOutType::file)
    {
        if(!ofs_.is_open() || ofs_.bad())
        {
            ofs_.close();
            ofs_.open(file_path_,std::ios_base::out|std::ios_base::app);
        }


        ofs_ << str << std::endl;

        if(ofs_.tellp() > max_size_m_*1024*1024)
        {
            ofs_.close();
           
            rename(file_path_.c_str(),get_bak_file_name().c_str());

            ofs_.open(file_path_,std::ios_base::out|std::ios_base::app);
        }

    }   
}

void Logger::format_log(int level,const char* file,uint32_t line,const char* format,...) 
{
    if(!startd_)
    {
        return;
    }
    
    if(0 == (level&log_level_mask_))
    {
        return;
    }

    if(level_map_.end() == level_map_.find(level))
    {
        return;
    }
   
    std::vector<char> buffer(128,'\0');

    va_list args;
    va_start(args,format);
    unsigned int len = vsnprintf(&buffer[0],buffer.size(),format,args);
    if(len >= buffer.size())
    {
        buffer.resize(len+1,'\0');

        va_list args1;
        va_start(args1,format);
        vsnprintf(&buffer[0],len,format,args1);
        va_end(args1);
    }
    va_end(args);
    buffer[len]='\0';

    std::vector<char> buffer2(256,'\0');

    auto tp_now = std::chrono::system_clock::now();    
    time_t t = std::chrono::system_clock::to_time_t(tp_now);
    struct tm* tm = std::localtime(&t);
    long ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp_now.time_since_epoch()).count() - 1000*t;

    len = snprintf(&buffer2[0],buffer2.size(),"{\"time\":\"%d-%02d-%02d %02d:%02d:%02d.%ld\",\"thread\":%s,\"location\":\"%s(%d)\",\"level\":\"%s\",\"msg\":\"%s\"}"
        ,1900+tm->tm_year,1+tm->tm_mon,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ms,this_thread_id_str().c_str(),file,line,level_map_[level].c_str(),buffer.data());
    if(len >= buffer2.size())
    {
        buffer2.resize(len+1,'\0');
        snprintf(&buffer2[0],len,"{\"time\":\"%d-%02d-%02d %02d:%02d:%02d.%ld\",\"thread\":%s,\"location\":\"%s(%d)\",\"level\":\"%s\",\"msg\":\"%s\"}"
            ,1900+tm->tm_year,1+tm->tm_mon,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ms,this_thread_id_str().c_str(),file,line,level_map_[level].c_str(),buffer.data());
    }
    buffer2[len]='\0';


    if(!async_)
    {
        std::lock_guard<std::mutex> lck(wrt_mutex_);
        log(buffer2.data(),level);
    }
    else
    {
        write_task_thread_->async_task(std::bind(&Logger::log,this,std::string(buffer2.data(),len),level));
    }

}

void Logger::raw_log(int level,const std::string& msg) 
{

    if(!startd_)
    {
        return;
    }

    if(0 == (level&log_level_mask_))
    {
        return;
    }

    if(level_map_.end() == level_map_.find(level))
    {
        return;
    }

    if(!async_)
    {
        std::lock_guard<std::mutex> lck(wrt_mutex_);
        log(msg,0xF);
    }
    else
    {
        write_task_thread_->async_task(std::bind(&Logger::log,this,msg,0xF));
    }
}


std::string Logger::this_thread_id_str() 
{    
    std::ostringstream oss;
    oss << std::this_thread::get_id();
   
    return oss.str();
}
    

std::string Logger::get_bak_file_name() 
{
    auto tp_now = std::chrono::system_clock::now();
    
    time_t t = std::chrono::system_clock::to_time_t(tp_now);
    struct tm* tm = std::localtime(&t);
        
    auto d_seconds = std::chrono::duration_cast<std::chrono::seconds>(tp_now.time_since_epoch());
    auto d_millis = std::chrono::duration_cast<std::chrono::milliseconds>(tp_now.time_since_epoch());

    auto dms = d_millis - d_seconds;

    std::ostringstream oss;
    oss << file_path_ << std::put_time(tm,"%Y-%m-%d_%H:%M:%S") << "." << dms.count();

    return oss.str();
}

}}