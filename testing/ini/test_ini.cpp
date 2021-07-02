#include "ini/ini.h"
#include "fstream"
#include "chrono"
#include <iostream>
#include <time.h>

int test_trim()
{
    std::string left_trim("     nihao   ");
    std::string left_expect("nihao   ");

    stdx::ini::Ini::trim_left(left_trim);
    if(left_trim != left_expect)
    {
        return -1;
    }

    std::string right_trim("     ni hao   ");
    std::string right_expect("     ni hao");
    stdx::ini::Ini::trim_right(right_trim);
    if(right_trim != right_expect)
    {
        return -2;
    }

    right_trim=" ni hao \r\n  ";
    right_expect=" ni hao";
    stdx::ini::Ini::trim_right(right_trim);
    if(right_trim != right_expect)
    {
        return -3;
    }

    std::string trim_str(" ni hao \r");
    std::string trim_expect("ni hao");
    stdx::ini::Ini::trim_string(trim_str);
    if(trim_str != trim_expect)
    {
        return -4;
    }

    std::string trim_empty("       ");
    stdx::ini::Ini::trim_string(trim_empty);
    if(!trim_empty.empty())
    {
        return -5;
    }

    trim_empty = "    \r\n  ";
    stdx::ini::Ini::trim_string(trim_empty);
    if(!trim_empty.empty())
    {
        return -6;
    }
    
    return 0;
}

int test_str()
{
    std::string s("[default]\nstr=string\nint32=32\nint64=64\n[section2]\n#comment\n;comment2\nkey1=value1\n");

    stdx::ini::Ini ini;
    ini.load_str(s);

    if(ini.get_str("default","str").second != "string" || ini.get_str("default","str").first != true)
    {
        return -1;
    }

    if(ini.get_int32("default","int32").first != true || ini.get_int32("default","int32").second != 32)
    {
        return -2;
    }

    if(ini.get_int32("default","int64").first != true || ini.get_int32("default","int64").second != 64)
    {
        return -3;
    }

    if(ini.get_str("section2","key1").first != true || ini.get_str("section2","key1").second != "value1")
    {
        return -4;
    }

    if(ini.get_str("section","key1").first == true)
    {
        return -5;
    }

    if(ini.get_str("","key1").first == true)
    {
        return -6;
    }

     if(ini.get_str("default","keyaafaf").first == true)
    {
        return -7;
    }

    return 0;
}

int test_file()
{
    std::string s("[default]\nstr=string\nint32=32\nint64=64\n[section2]\n#comment\n;comment2\nkey1=value1\n");

    std::remove("test.ini");
    std::ofstream ofs("test.ini",std::ios_base::out|std::ios_base::trunc);
    ofs << s;
    ofs.close();

    stdx::ini::Ini ini;
    if(!ini.load_file("test.ini"))
    {
        return -100;
    }

    if(ini.get_str("default","str").second != "string" || ini.get_str("default","str").first != true)
    {
        return -1;
    }

    if(ini.get_int32("default","int32").first != true || ini.get_int32("default","int32").second != 32)
    {
        return -2;
    }

    if(ini.get_int32("default","int64").first != true || ini.get_int32("default","int64").second != 64)
    {
        return -3;
    }

    if(ini.get_str("section2","key1").first != true || ini.get_str("section2","key1").second != "value1")
    {
        return -4;
    }

    if(ini.get_str("section","key1").first == true)
    {
        return -5;
    }

    if(ini.get_str("","key1").first == true)
    {
        return -6;
    }

    if(ini.get_str("default","keyaafaf").first == true)
    {
        return -7;
    }

    return 0;
}

int test_tolower()
{
    std::string in("00ABCDefgHijk021~2)");
    std::string expect("00abcdefghijk021~2)");

    stdx::ini::Ini::to_lower(in);

    if(in != expect)
    {
        return -1;
    }

    return 0;
}

int test_save()
{
    stdx::ini::Ini ini;
    ini.set_bool("","default-true",true);
    ini.set_bool("","default-false",false);
    ini.set_int32("","default-int",32);
    ini.set_int64("","default-int64",64);
    ini.set_str("","default-str","dstr");

    ini.set_bool("section1","1-true",true);
    ini.set_int32("section1","i-32",32);
    ini.set_str("section1","1-str","1string");

    ini.set_str("section2","","");

    ini.save_to_file("test.ini");

    if(ini.set_bool("section2","",true))
    {
        return -1;
    }

    if(ini.set_int32("section2","",1))
    {
        return -2;
    }

    if(ini.set_int64("section2","",1))
    {
        return -2;
    }

    if(ini.set_str("section2","","ss"))
    {
        return -3;
    }

    stdx::ini::Ini ini1;
    if(!ini1.load_file("test.ini"))
    {
        return -4;
    }

    if(!ini1.get_bool("","default-true").second)
    {
        return -5;
    }

    if(ini1.get_int32("","default-int").second != 32)
    {
        return -6;
    }

    if(ini1.get_int32("","").first)
    {
        return -7;
    }

    if(ini1.get_str("section1","1-str").second != "1string")
    {
        return -8;
    }

    return 0;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
    //auto tp_now = std::chrono::system_clock::now();
    
   // time_t t = std::chrono::system_clock::to_time_t(tp_now);
    //struct tm* tm = std::localtime(&t);
  //  int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp_now.time_since_epoch()).count();
  //  int64_t s = t;

  //  std::cout << "seconds" << s << "miliseconds:" << ms << std::endl;

        test_tolower();
       // test_save();
        return 0;

    }else
    {
        std::string cmd(argv[1]);
        

        if("str" == cmd)
        {
            return test_str();
        }

        if("file" == cmd)
        {
            return test_file();
        }

        if("trim" == cmd)
        {
            return test_trim();
        }

        if("lower" == cmd)
        {
            return test_tolower();
        }

        if("save" == cmd)
        {
            return test_save();
        }

    }


    return -200;

}