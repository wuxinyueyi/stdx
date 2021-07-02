/*
a INI wrapper
*/
#ifndef __STDX_INI_H__
#define __STDX_INI_H__

#include <string>
#include <map>

#include <stdint.h>

namespace stdx{namespace ini{

class Ini
{
    public:     
        Ini();      
        virtual ~Ini();

    public:
        //load from file
        bool load_file(const std::string& file_path);
        //load from string
        bool load_str(const std::string& buffer);

        std::pair<bool,int32_t> get_int32(const std::string& section,const std::string& key);
        bool set_int32(const std::string& section,const std::string& key, int32_t val);

        std::pair<bool,int64_t> get_int64(const std::string& section,const std::string& key);
        bool set_int64(const std::string& section, const std::string& key, int64_t val);
        
        std::pair<bool,bool> get_bool(const std::string& section,const std::string& key);
        bool set_bool(const std::string& section,const std::string&key, bool val);

        std::pair<bool,std::string> get_str(const std::string& section,const std::string& key);
        bool set_str(const std::string& section,const std::string& key, const std::string& val);

        bool save_to_file(const std::string& file_path);
        //output to a ini format string;
        std::string to_str();

        static void trim_left(std::string& str);
        static void trim_right(std::string& str);
        //trim both left and right
        static void trim_string(std::string& str);
        static void to_lower(std::string& str);

    private:    
        
        //stection,key,value
        std::map<std::string,std::map<std::string,std::string>> map_;

        std::string DEFAULT_SECTION;
};

}}

#endif