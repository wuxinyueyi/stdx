#include "tcp/packet.h"

#ifdef WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif // WINDOWS


namespace stdx{ namespace tcp{



    std::vector<char> serialize(const char* data, int32_t len,const std::shared_ptr<PacketHead>& head)
    {
        std::vector<char> v;
        if(nullptr == head)
        {
            return v;
        }

        v.resize(sizeof(PacketHead)+len,'\0');

        PacketHead* p = (PacketHead*)&v[0];
       
        p->tag_=htonl(head->tag_);
        p->type_ = htonl(head->type_);
        p->id_= htonl(head->id_);
        p->time_ = htonl(head->time_);
        p->reserve_=htonl(head->reserve_);
        p->reserve1_ = htonl(head->reserve1_);
        p->crc_= htonl(0);//todo:not used yet
        p->length_= htonl(len);
       
        for(int i(0); i < len; ++i)
        {
            if(NULL != data)
            {
                v[sizeof(PacketHead)+i] = *(data+i);   
            }
        }


        return v;
    }

    std::shared_ptr<Packet> unserialize(std::vector<char>& buffer)
    {
        if(buffer.size() < sizeof(PacketHead))
        {
            return nullptr;
        }

        std::shared_ptr<Packet> p_ptr = nullptr;

        unsigned int pos(0);
        bool found_tag(false);
        bool wait_more(false);

        while (pos != buffer.size())
        {
            if(pos + sizeof(PacketHead) > buffer.size())
            {
                wait_more = true;
                break;
            }

            PacketHead *p = (PacketHead*)&buffer[pos];
            if(ntohl(p->tag_) != DEFAULT_PACKET_TAG)
            {
                ++pos;
                continue;
            }else            
            {
                found_tag = true;        

                int32_t len = ntohl(p->length_);
                if(pos+sizeof(PacketHead) + len > buffer.size())
                {
                    wait_more = true;
                    break;
                }

                p_ptr = std::make_shared<Packet>();
                p_ptr->head_.tag_ = DEFAULT_PACKET_TAG;
                p_ptr->head_.type_ = ntohl(p->type_);
                p_ptr->head_.id_ = ntohl(p->id_);
                p_ptr->head_.time_ = ntohl(p->time_);
                p_ptr->head_.reserve_ = ntohl(p->reserve_);
                p_ptr->head_.reserve1_ = ntohl(p->reserve1_);
                p_ptr->head_.crc_ = ntohl(p->crc_);
                p_ptr->head_.length_ = ntohl(p->length_);
                p_ptr->data_.insert(p_ptr->data_.begin(),buffer.begin()+pos+sizeof(PacketHead),buffer.begin()+pos+sizeof(PacketHead)+len);                

                break;
            }
        }

        if(found_tag)
        {
            if(!wait_more)//if got whole data
            {
                buffer.erase(buffer.begin(), buffer.begin()+pos+sizeof(PacketHead)+p_ptr->head_.length_);
            }else //need more data, wait next time,erase invalid data that ahead of tag
            {
                buffer.erase(buffer.begin(),buffer.begin()+pos);
            }
          
        }else
        {
            if(!wait_more)
            {
                buffer.clear();
            }
        }
        
        return p_ptr;
    }
    
}}