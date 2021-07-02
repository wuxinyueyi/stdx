/*
define a common used, user defined protocal

*/
#ifndef __STDX_TCP_PACKAGE_H__
#define __STDX_TCP_PACKAGE_H__

#include <cstdint>
#include <vector>
#include <memory>

namespace stdx{namespace tcp{

#define DEFAULT_PACKET_TAG 0x2F2F2F2F


const int32_t kPacketTypeMask = 0xF0000000;



//type heart beat 
const int32_t kDefaultHeartbeatReq = 0x01;
const int32_t kDefaultHeartbeatAck = kPacketTypeMask|kPacketTypeMask;


//Head of each packet
struct PacketHead
{
    //the tag specify the begining fo each package
    int32_t tag_; 
    //generally, means the package type
    int32_t type_;
    //generally, means the sequnce number
    int32_t id_;
    //generally, the time_t value of the send time
    int32_t time_;
    //reservation
    int32_t reserve_;
    int32_t reserve1_;
    //the crc value of the data
    int32_t crc_;
    //the length of the data
    int32_t length_;   

    PacketHead():tag_(DEFAULT_PACKET_TAG),type_(0),id_(0),reserve_(0),reserve1_(0),crc_(0),length_(0){}
    PacketHead(int32_t type,int32_t id):tag_(DEFAULT_PACKET_TAG),type_(type),id_(id),reserve_(0),reserve1_(0),crc_(0),length_(0){}
    PacketHead(int32_t type,int32_t id,int32_t reserve,int32_t reserve1):tag_(DEFAULT_PACKET_TAG),type_(type),id_(id),reserve_(reserve),reserve1_(reserve1),crc_(0),length_(0){}
};

struct Packet
{
    PacketHead head_;
    //generally,a JSON string
    std::vector<char> data_; 
};

//serialize a Packet to a stream
std::vector<char> serialize(const char* data, int32_t len, const std::shared_ptr<PacketHead>& head);

//unserialize from stream to a Packet struct
std::shared_ptr<Packet> unserialize(std::vector<char>& buffer);



}}

#endif