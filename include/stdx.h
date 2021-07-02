/*
if you want stdx to output logs, you have to set a log function by calling stdx::set_stdx_log_func
*/
#ifndef __STDX_STDX_H__
#define __STDX_STDX_H__

#include <thread>
#include <functional>
#include <sstream>

namespace stdx{
    //the global function that stdx will take to write logs
    //fn is the log function 
    void set_stdx_log_func(std::function<void(int level,const std::string& msg)> fn);
    
    std::function<void(int level,const std::string& msg)> get_stdx_log_function();

    std::string time_str();
    std::string file_name(const std::string& path);

	enum 
	{
		kInfo = 0x1,
		kDebug = 0x2,
		kWarn = 0x4,
		kError = 0x8,
		kFatal = 0x10,
	};
    
}

#define stdxLogError(Errmsg)\
do{\
    if(!get_stdx_log_function())\
    {\
        break;\
    }\
    std::ostringstream ossstdx;\
    ossstdx << "{\"time\":\"" << stdx::time_str() << "\",\"thread\":" << std::this_thread::get_id() <<",\"location\":\"" << stdx::file_name(__FILE__) <<"(" << __LINE__ <<")\",\"level\":\"Error\",\"msg\":\"" << Errmsg << "\"}";\
    get_stdx_log_function()(kError,ossstdx.str());\
}while(0);\

#define stdxLogWarn(Warnmsg)\
do{\
    if(!get_stdx_log_function())\
    {\
        break;\
    }\
    std::ostringstream ossstdx;\
    ossstdx << "{\"time\":\"" << stdx::time_str() << "\",\"thread\":" << std::this_thread::get_id() <<",\"location\":\"" << stdx::file_name(__FILE__) <<"(" << __LINE__ <<")\",\"level\":\"Warn\",\"msg\":\"" << Warnmsg << "\"}";\
    get_stdx_log_function()(kWarn,ossstdx.str());\
}while(0);\

#define stdxLogDebug(Debugmsg)\
do{\
    if(!get_stdx_log_function())\
    {\
        break;\
    }\
    std::ostringstream ossstdx;\
    ossstdx << "{\"time\":\"" << stdx::time_str() << "\",\"thread\":" << std::this_thread::get_id() <<",\"location\":\"" << stdx::file_name(__FILE__) <<"(" << __LINE__ <<")\",\"level\":\"Debug\",\"msg\":\"" << Debugmsg << "\"}";\
    get_stdx_log_function()(kDebug,ossstdx.str());\
}while(0);\

#define stdxLogInfo(Infomsg)\
do{\
    if(!get_stdx_log_function())\
    {\
        break;\
    }\
    std::ostringstream ossstdx;\
    ossstdx << "{\"time\":\"" << stdx::time_str() << "\",\"thread\":" << std::this_thread::get_id() <<",\"location\":\"" << stdx::file_name(__FILE__) <<"(" << __LINE__ <<")\",\"level\":\"Debug\",\"msg\":\"" << Infomsg << "\"}";\
    get_stdx_log_function()(kDebug,ossstdx.str());\
}while(0);\


#endif