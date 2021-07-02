/*
handy log macros
*/
#ifndef __STDX_LOG_LOG_H__
#define __STDX_LOG_LOG_H__

#include "log/logger.h"
namespace stdx{
    std::string file_name(const std::string& path);
}


//call this macro once at the beginning
#define LOG_START stdx::log::Logger::instance().start

//to stop log output,close log file
#define LOG_STOP stdx::log::Logger::instance().stop
    

#define LOG_INFO(fmt,...) \
do{\
stdx::log::Logger::instance().format_log(stdx::log::ELogLevel::info,stdx::file_name(__FILE__).c_str(),__LINE__,fmt,##__VA_ARGS__);\
}while(0);\

#define LOG_DEBUG(fmt,...) \
do{\
stdx::log::Logger::instance().format_log(stdx::log::ELogLevel::debug,stdx::file_name(__FILE__).c_str(),__LINE__,fmt,##__VA_ARGS__);\
}while(0);\

#define LOG_WARN(fmt,...) \
do{\
stdx::log::Logger::instance().format_log(stdx::log::ELogLevel::warn,stdx::file_name(__FILE__).c_str(),__LINE__,fmt,##__VA_ARGS__);\
}while(0);\

#define LOG_ERROR(fmt,...) \
do{\
stdx::log::Logger::instance().format_log(stdx::log::ELogLevel::error,stdx::file_name(__FILE__).c_str(),__LINE__,fmt,##__VA_ARGS__);\
}while(0);\

#define LOG_FATAL(fmt,...) \
do{\
stdx::log::Logger::instance().format_log(stdx::log::ELogLevel::fatal,stdx::file_name(__FILE__).c_str(),__LINE__,fmt,##__VA_ARGS__);\
}while(0);\

//int level，std::string msg
#define LOG_RAW(level,msg)\
do{\
stdx::log::Logger::instance().raw_log(level,msg);\
}while(0);\


#endif
