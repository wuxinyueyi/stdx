/*
a tcp linker, means a connection,this object hold the ip,port,socket and data buffer of a connection.
*/
#ifndef __STDX_TCP_TCPLINKER_H_
#define __STDX_TCP_TCPLINKER_H_


#include <string>
#include <chrono>
#include <vector>
#include <memory>
#include <mutex>

namespace stdx{namespace tcp{

struct PacketHead;
struct Packet;

class TcpLinker
{
    public:
        TcpLinker(int fd,unsigned short port,std::string ip);
        ~TcpLinker();

        unsigned short port();
        int fd();
        std::string ip();
#ifndef WIN32
        //directly send plain data to socekt
        int send_data(const char* data, unsigned int len);       
        //make a Packet struct stream,then send the stream to the socket
        int send_packet_data(const char* data, int len, const std::shared_ptr<PacketHead>& head_ptr);
#endif//!WIN32

        inline void update_alive_time(){last_alive_time_ = std::chrono::system_clock::now();};
        inline  std::chrono::system_clock::time_point last_alive_time(){return last_alive_time_;};

        //append data received to the buffer_,not thread safe
        bool append_buffer(const std::vector<char>& buffer);

		bool append_buffer(const char* data, int len);
        
        //unserialize a Packet from the buffer_,not trhead safe
        std::shared_ptr<Packet> get_packet();
    private:
        int fd_;
        unsigned short port_;
        std::string ip_;
        
        std::chrono::system_clock::time_point last_alive_time_;

        //the data buffer read from socket
        std::vector<char> buffer_;
#ifdef WIN32
		std::mutex mtx_buffer_;
#endif // WIN32


        const unsigned int MAX_BUFFER_SIZE = 1024*1024*4;//4M
};
}}

#endif