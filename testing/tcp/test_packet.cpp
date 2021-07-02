#include "tcp/packet.h"
#include <string>


int test_serialize()
{
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(2,1);
    std::string in("hello");
    std::vector<char> v = stdx::tcp::serialize(in.data(),in.size(),head_ptr);
    if(v.size() != sizeof(stdx::tcp::PacketHead)+in.size())
    {
        return -1;
    }

    std::string out(v.begin()+sizeof(stdx::tcp::PacketHead),v.end());
    if(out != in)
    {
        return -2;
    }

    return 0;
    
}

int test_serialize1()
{   
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(2,1);
    std::vector<char> v = stdx::tcp::serialize(NULL,0,head_ptr);
    if(v.size() != sizeof(stdx::tcp::PacketHead))
    {
        return -1;
    }

    std::string out(v.begin()+sizeof(stdx::tcp::PacketHead),v.end());
    if(!out.empty())
    {
        return -2;
    }

    std::vector<char> v1 = stdx::tcp::serialize(NULL,10,head_ptr);
    if(v1.size() != sizeof(stdx::tcp::PacketHead)+10)
    {
        return -1;
    }

    std::string out1(v1.begin()+sizeof(stdx::tcp::PacketHead),v1.end());
    if(out1.empty())
    {
        return -2;
    }

    return 0;
    
}

int test_serialize2()
{
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(2,1);
    std::string in("hello");
    std::vector<char> v = stdx::tcp::serialize(in.data(),100,head_ptr);
    if(v.size() == sizeof(stdx::tcp::PacketHead)+in.size())
    {
        return -1;
    }

    std::string out(v.begin()+sizeof(stdx::tcp::PacketHead),v.end());
    if(out == in)
    {
        return -2;
    }

    return 0;
    
}

int test_unserialize()
{
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(123,1);
    std::string h("hello");

    std::vector<char> v = stdx::tcp::serialize(h.data(),h.size(),head_ptr); 

    auto pkt = stdx::tcp::unserialize(v);

    std::string s(pkt->data_.begin(),pkt->data_.end());

    if(s != "hello")
    {
        return -1;
    }

    if(pkt->head_.tag_ != DEFAULT_PACKET_TAG)
    {
        return -2;

    }
    
    if(pkt->head_.type_ != 123)
    {
        return -3;
    }

    if((__uint32_t)pkt->head_.length_ != h.size())
    {
        return -4;
    }   

    return 0;
}


int test_unserialize1()
{
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(123,1);

    std::string h("hello");

    std::vector<char> v = stdx::tcp::serialize(h.data(),h.size(),head_ptr); 
    
    std::string w("world");
    v.insert(v.begin()+v.size(),w.begin(),w.end());

    auto pkt = stdx::tcp::unserialize(v);

    std::string s(pkt->data_.begin(),pkt->data_.end());

    if(s != "hello")
    {
        return -1;
    }

    if(pkt->head_.tag_ != DEFAULT_PACKET_TAG)
    {
        return -2;

    }
    
    if(pkt->head_.type_ != 123)
    {
        return -3;
    }

    if((__uint32_t)pkt->head_.length_ != h.size())
    {
        return -4;
    }   

    

    
    std::string e(v.begin(),v.end());
    if(e!=w)
    {
        return -5;
    }

    return 0;
}

int test_unserialize2()
{
    auto head_ptr = std::make_shared<stdx::tcp::PacketHead>(123,1);

    std::vector<char> v1;
    v1.push_back('d');
    v1.push_back('i');
    v1.push_back('r');
    v1.push_back('t');
    v1.push_back('y');

    std::string h("hello");

    std::vector<char> v = stdx::tcp::serialize(h.data(),h.size(),head_ptr); 

    v1.insert(v1.begin()+v1.size(),v.begin(),v.end());

    v1.push_back('d');
    v1.insert(v1.begin()+v1.size(),v.begin(),v.end());
    

    auto pkt = stdx::tcp::unserialize(v1);

    std::string s(pkt->data_.begin(),pkt->data_.end());

    if(s != "hello")
    {
        return -1;
    }

    if(pkt->head_.tag_ != DEFAULT_PACKET_TAG)
    {
        return -2;

    }
    
    if(pkt->head_.type_ != 123)
    {
        return -3;
    }

    if((__uint32_t)pkt->head_.length_ != h.size())
    {
        return -4;
    }   


    //////////////////////////////////////
    auto pkt1 = stdx::tcp::unserialize(v1);

    std::string s1(pkt1->data_.begin(),pkt1->data_.end());   

    if(s1 != "hello")
    {
        return -1;
    }

    if(pkt1->head_.tag_ != DEFAULT_PACKET_TAG)
    {
        return -2;

    }
    
    if(pkt1->head_.type_ != 123)
    {
        return -3;
    }

    if((__uint32_t)pkt1->head_.length_ != h.size())
    {
        return -4;
    }   
    
   

    return 0;
}

int main(int arc, char ** argv)
{

    if(arc < 2)//debug
    {
        test_serialize1();

    }else
    {
        std::string cmd(argv[1]);
        if(cmd == "s")
        {
            return test_serialize();
        }

        if(cmd == "s1")
        {
            return test_serialize1();
        }

        if(cmd == "s2")
        {
            return test_serialize2();
        }

        if(cmd == "u")
        {
            return test_unserialize();
        }

        if(cmd == "u1")
        {
            return test_unserialize1();
        }

        if(cmd == "u2")
        {
            return test_unserialize2();
        }
    }

    

}