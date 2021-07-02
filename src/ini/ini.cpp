#include "ini/ini.h"
#include "fstream"
#include <sstream>

namespace stdx{namespace ini{

Ini::Ini():DEFAULT_SECTION("`DEFAULT_SECTION`") 
{    
}

Ini::~Ini() 
{    
}

bool Ini::load_file(const std::string& file_path) 
{
    std::ifstream ifs(file_path,std::ios_base::in);
    std::ostringstream oss;

    if(ifs)
    {
        oss << ifs.rdbuf();
    }

    ifs.close();

    return load_str(oss.str());

}

bool Ini::load_str(const std::string& str) 
{
    if(str.empty())
    {
        return false;
    }

    size_t pos_begin(0);
    size_t pos = str.find_first_of('\n',pos_begin);

    std::string buffer;
    std::string cur_section(DEFAULT_SECTION);

    while (pos != std::string::npos)
    {
        buffer = str.substr(pos_begin,pos-pos_begin);

        trim_string(buffer);

        if(!buffer.empty() 
            &&('#' != buffer[0])
            &&(';' != buffer[0])
        )
        {
            if('[' == buffer[0])//section
            {
                auto p = buffer.find_first_of(']');
                if(p != std::string::npos)
                {
                    cur_section = buffer.substr(1,p-1);                    
                }
            }
            else //key value
            {
                auto p = buffer.find_first_of('=');
                if(p != std::string::npos)
                {
                    std::string key = buffer.substr(0,p);
                    trim_string(key);
                    std::string value = buffer.substr(p+1);
                    trim_string(value);

                    auto it = map_.find(cur_section);
                    if(it == map_.end())
                    {
                        map_.emplace(cur_section,std::map<std::string,std::string>());
                    }

                    it = map_.find(cur_section);
                    if(it == map_.end())
                    {
                        return false;
                    }

                    it->second[key] = value;
                }

            }

        }       

        pos_begin = pos+1;

        pos = str.find_first_of('\n',pos_begin);
    }
    

    return true;
}


std::pair<bool,int32_t> Ini::get_int32(const std::string& section,const std::string& key) 
{
    if(key.empty())
    {
        return std::pair<bool,int32_t>(false,0);
    }
    
    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        return std::pair<bool,int32_t>(false,0);
    }

    auto it1 = it->second.find(key);
    if(it1 == it->second.end())
    {
        return std::pair<bool,int32_t>(false,0);
    }

    return std::pair<bool,int32_t>(true,std::stol(it1->second));
}

bool Ini::set_int32(const std::string& section,const std::string& key, int32_t val) 
{
    if(key.empty())
    {
        return false;
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        map_.emplace(sec,std::map<std::string,std::string>());
    }

    it = map_.find(sec);
    if(it == map_.end())
    {
        return false;
    }

    it->second[key] = std::to_string(val);

    return true;
}

std::pair<bool,int64_t> Ini::get_int64(const std::string& section,const std::string& key) 
{
    if(key.empty())
    {
        return std::pair<bool,int32_t>(false,0);
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        return std::pair<bool,int32_t>(false,0);
    }

    auto it1 = it->second.find(key);
    if(it1 == it->second.end())
    {
        return std::pair<bool,int32_t>(false,0);
    }

    return std::pair<bool,int32_t>(true,std::stoll(it1->second));
}

bool Ini::set_int64(const std::string& section, const std::string& key, int64_t val) 
{
    if(key.empty())
    {
        return false;
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        map_.emplace(sec,std::map<std::string,std::string>());
    }

    it = map_.find(sec);
    if(it == map_.end())
    {
        return false;
    }

    it->second[key] = std::to_string(val);

    return true;
}

std::pair<bool,bool> Ini::get_bool(const std::string& section,const std::string& key) 
{
    if(key.empty())
    {
        return std::pair<bool,bool>(false,false);
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        return std::pair<bool,bool>(false,false);
    }

    auto it1 = it->second.find(key);
    if(it1 == it->second.end())
    {
        return std::pair<bool,bool>(false,false);
    }

    bool ret(false);
    std::string tmp(it1->second);
    to_lower(tmp);
    if(tmp == "true" || tmp == "1")
    {
        ret = true;
    }

    return std::pair<bool,bool>(true,ret);
}

bool Ini::set_bool(const std::string& section,const std::string&key, bool val) 
{
    if(key.empty())
    {
        return false;
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        map_.emplace(sec,std::map<std::string,std::string>());
    }

    it = map_.find(sec);
    if(it == map_.end())
    {
        return false;
    }

    it->second[key] = val?"true":"false";

    return true;
}

std::pair<bool,std::string> Ini::get_str(const std::string& section,const std::string& key) 
{
    if(key.empty())
    {
        return std::pair<bool,std::string>(false,"");
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        return std::pair<bool,std::string>(false,"");
    }

    auto it1 = it->second.find(key);
    if(it1 == it->second.end())
    {
        return std::pair<bool,std::string>(false,"");
    }

    return std::pair<bool,std::string>(true,it1->second);
}

bool Ini::set_str(const std::string& section,const std::string& key, const std::string& val) 
{
    if(key.empty())
    {
        return false;
    }

    std::string sec(section);
    if(sec.empty())
    {
        sec = DEFAULT_SECTION;
    }

    auto it = map_.find(sec);
    if(it == map_.end())
    {
        map_.emplace(sec,std::map<std::string,std::string>());
    }

    it = map_.find(sec);
    if(it == map_.end())
    {
        return false;
    }

    it->second[key] = val;

    return true;
}

bool Ini::save_to_file(const std::string& file_path) 
{
    std::ofstream ofs(file_path,std::ios_base::out|std::ios_base::trunc);

    if(!ofs)
    {
        return false;
    }

    auto it = map_.find(DEFAULT_SECTION);
    if(it != map_.end())
    {
        for(auto v:it->second)
        {
            ofs << v.first << "=" << v.second << std::endl;
        }
    }

    for(auto v:map_)
    {
        if(v.first == DEFAULT_SECTION)
        {
            continue;
        }
        
        ofs << "[" << v.first << "]" << std::endl;

        for(auto v1:v.second)
        {
            ofs << v1.first << "=" << v1.second << std::endl;
        }
    }

    ofs.close();
    return true;
}

std::string Ini::to_str() 
{
    std::ostringstream oss;

    auto it = map_.find(DEFAULT_SECTION);
    if(it != map_.end())
    {
        for(auto v:it->second)
        {
            oss << v.first << "=" << v.second << std::endl;
        }
    }

    for(auto v:map_)
    {
        if(v.first == DEFAULT_SECTION)
        {
            continue;
        }
        
        oss << "[" << v.first << "]" << std::endl;

        for(auto v1:v.second)
        {
            oss << v1.first << "=" << v1.second << std::endl;
        }
    }

    return oss.str();
}



void Ini::trim_left(std::string& str) 
{
    auto it = str.begin();
    while(it != str.end())
    {
        if(*(it) == ' ')
        {
            it = str.erase(it);
        }else
        {
            break;
        }
    }
}

void Ini::trim_right(std::string& str) 
{
    
    int pos = str.size()-1;
    while (pos >= 0)
    {
        if((' ' == str[pos])
        || ('\n' == str[pos])
        || ('\r' == str[pos])
        )
        {
            str.erase(pos);
            pos = str.size() -1;
        }else
        {
            break;
        }
    }    
    
}

void Ini::trim_string(std::string& str) 
{
    trim_left(str);
    trim_right(str);
}

void Ini::to_lower(std::string& str) 
{
    for(auto it = str.begin(); it != str.end(); ++it)
    {
        (*it) = tolower(*it);
    }
}



}}