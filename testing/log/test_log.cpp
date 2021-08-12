#include "log/log.h"
#include <string>
#include <stdio.h>

#ifdef WIN32
#include  <io.h>

#else
#include <unistd.h>
#endif //WIN32



int test_log_file()
{
    remove("./log_file.log");
    LOG_START("./log_file.log");

#ifdef WIN32
	int ret = _access("./log_file.log", 0);
#else
	int ret = access("./log_file.log", F_OK);
#endif // WIN32

	

    LOG_STOP();

    return ret;
}

int test_no_log_file()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",0xF,stdx::log::ELogOutType::console);

#ifdef WIN32
	int ret = _access("./log_file.log", 0);
#else
	int ret = access("./log_file.log", F_OK);
#endif // WIN32
    

    LOG_STOP();

    return ret==0?1:0;
}

int test_info_log()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",stdx::log::ELogLevel::info,0xF,256,false);

    LOG_INFO("THIS IA A INFO TEST");
    LOG_ERROR("this is a error test");
    LOG_WARN("this is a warn test");
    LOG_DEBUG("this is a debug test");
    
    LOG_STOP();

    std::ifstream ifs("./log_file.log",std::ios_base::in);
    ifs.seekg(0,ifs.end);
    int len = ifs.tellg();
    ifs.seekg(0,ifs.beg);

    std::vector<char> buffer(len+1,'\0');
    ifs.read(&buffer[0],len);
    ifs.close();
    
    std::string s(buffer.begin(),buffer.end());

    if(s.find("THIS IA A INFO TEST") == std::string::npos)
    {
        return 1;
    }

    if(s.find("this is a error test") != std::string::npos)
    {
        return 2;
    }

    if(s.find("this is a warn test") != std::string::npos)
    {
        return 3;
    }

    if(s.find("this is a debug test") != std::string::npos)
    {
        return 4;
    }

    return 0;
}

int test_debug_log()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",stdx::log::ELogLevel::debug,0xF,256,false);

    LOG_INFO("THIS IA A INFO TEST");
    LOG_ERROR("this is a error test");
    LOG_WARN("this is a warn test");
    LOG_DEBUG("this is a debug test");
    
    LOG_STOP();

    std::ifstream ifs("./log_file.log",std::ios_base::in);
    ifs.seekg(0,ifs.end);
    int len = ifs.tellg();
    ifs.seekg(0,ifs.beg);

    std::vector<char> buffer(len+1,'\0');
    ifs.read(&buffer[0],len);
    ifs.close();
    
    std::string s(buffer.begin(),buffer.end());

    if(s.find("THIS IA A INFO TEST") != std::string::npos)
    {
        return 1;
    }

    if(s.find("this is a error test") != std::string::npos)
    {
        return 2;
    }

    if(s.find("this is a warn test") != std::string::npos)
    {
        return 3;
    }

    if(s.find("this is a debug test") == std::string::npos)
    {
        return 4;
    }

    return 0;
}

int test_warn_log()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",stdx::log::ELogLevel::warn,0xf,256,false);

    LOG_INFO("THIS IA A INFO TEST");
    LOG_ERROR("this is a error test");
    LOG_WARN("this is a warn test");
    LOG_DEBUG("this is a debug test");
    
    LOG_STOP();

    std::ifstream ifs("./log_file.log",std::ios_base::in);
    ifs.seekg(0,ifs.end);
    int len = ifs.tellg();
    ifs.seekg(0,ifs.beg);

    std::vector<char> buffer(len+1,'\0');
    ifs.read(&buffer[0],len);
    ifs.close();
    
    std::string s(buffer.begin(),buffer.end());

    if(s.find("THIS IA A INFO TEST") != std::string::npos)
    {
        return 1;
    }

    if(s.find("this is a error test") != std::string::npos)
    {
        return 2;
    }

    if(s.find("this is a warn test") == std::string::npos)
    {
        return 3;
    }

    if(s.find("this is a debug test") != std::string::npos)
    {
        return 4;
    }

    return 0;
}

int test_error_log()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",stdx::log::ELogLevel::error,0xf,256,false);

    LOG_INFO("THIS IA A INFO TEST");
    LOG_ERROR("this is a error test");
    LOG_WARN("this is a warn tesst");
    LOG_DEBUG("this is a debug test");
    
    LOG_STOP();

    std::ifstream ifs("./log_file.log",std::ios_base::in);
    ifs.seekg(0,ifs.end);
    int len = ifs.tellg();
    ifs.seekg(0,ifs.beg);

    std::vector<char> buffer(len+1,'\0');
    ifs.read(&buffer[0],len);
    ifs.close();
    
    std::string s(buffer.begin(),buffer.end());

    if(s.find("THIS IA A INFO TEST") != std::string::npos)
    {
        return 1;
    }

    if(s.find("this is a error test") == std::string::npos)
    {
        return 2;
    }

    if(s.find("this is a warn test") != std::string::npos)
    {
        return 3;
    }

    if(s.find("this is a debug test") != std::string::npos)
    {
        return 4;
    }

    return 0;
}

int test_format_log()
{
    remove("./log_file.log");
    
    LOG_START("./log_file.log",0xf,0xf,256,false);

    LOG_INFO("format int %d",1);
    LOG_ERROR("format string %s int %d","str", 65535);
    LOG_WARN("format x 0x%x",65534);
    
    LOG_STOP();

    std::ifstream ifs("./log_file.log",std::ios_base::in);
    ifs.seekg(0,ifs.end);
    int len = ifs.tellg();
    ifs.seekg(0,ifs.beg);

    std::vector<char> buffer(len+1,'\0');
    ifs.read(&buffer[0],len);
    ifs.close();
    
    std::string s(buffer.begin(),buffer.end());

    if(s.find("format int 1") == std::string::npos)
    {
        return 1;
    }

    if(s.find("format string str int 65535") == std::string::npos)
    {
        return 2;
    }

    if(s.find("format x 0xfffe") == std::string::npos)
    {
        return 3;
    }

    return 0;
}

int main(int arc,char ** argv)
{
    if(arc<2)
    {    
       
       LOG_START("test.log");       

       LOG_INFO("this is a text tst:%s","test");
       LOG_ERROR("nihoa %d",12);
       LOG_WARN("this is a raw test ");

	   time_t t_now = time(NULL);
	   long df = t_now - 0;
	   std::string ip("127.0.0.1");
	   unsigned short port(8888);
	   LOG_ERROR("received data %s,tp[%d] diff[%ld] from %s:%d fd[%d] type[%d] id[%d]", " ", (int)1, df, ip.c_str(), port, 4, 5,6);

       LOG_STOP();

    }else
    {
        std::string cmd(argv[1]);
        if("file" == cmd)
        {
            return test_log_file();
        }
        if("nofile" == cmd)
        {
            return test_no_log_file();
        }
        if("info" == cmd)
        {
            return test_info_log();
        }
        if("debug" == cmd)
        {
            return test_debug_log();
        }
        if("warn" == cmd)
        {
            return test_warn_log();
        }
        if("error" == cmd)
        {
            return test_error_log();
        }
        if("fmt" == cmd)
        {
            return test_format_log();
        }
    }
}
