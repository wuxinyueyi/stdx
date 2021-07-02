#include "tcp/tcp_linker.h"
#include "stdx.h"
#include "tcp/packet.h"

#ifdef WIN32

#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif // WIN32



#include <string.h>

namespace stdx{namespace tcp{

    TcpLinker::TcpLinker(int fd,unsigned short port,std::string ip):fd_(fd),port_(port),ip_(ip),last_alive_time_(std::chrono::system_clock::now())
    {}
    TcpLinker::~TcpLinker(){}

    unsigned short TcpLinker::port()
    {
        return port_;
    }

    int TcpLinker::fd()
    {
        return fd_;
    }

    std::string TcpLinker::ip()
    {
         return ip_;
    }

#ifndef WIN32
	int TcpLinker::send_data(const char* data, unsigned int len)
	{
		int ret = send(fd_, data, len, 0);
		if (-1 == ret)
		{
			std::ostringstream oss;
			oss << "send to fd[" << fd_ << "] failed,err:" << strerror(errno);
			stdxLogWarn(oss.str());
		}
		return ret;
	}

	int TcpLinker::send_packet_data(const char* data, int len, const std::shared_ptr<PacketHead>& head_ptr)
	{
		std::vector<char> pkt_data = serialize(data, len, head_ptr);
		return send_data(pkt_data.data(), pkt_data.size());
	}

#endif // !WIN32

    

    bool TcpLinker::append_buffer(const std::vector<char>& buffer)
    {
#ifdef WIN32
		std::lock_guard<std::mutex> lck(mtx_buffer_);
#endif // WIN32

        if(buffer_.size() > MAX_BUFFER_SIZE)
        {
            std::ostringstream oss;
            oss << "Too big buffer size:[" << buffer_.size() << "]";
            stdxLogWarn(oss.str());
        }

        buffer_.insert(buffer_.begin()+buffer_.size(),buffer.begin(),buffer.end());

        return true;
    }

	bool TcpLinker::append_buffer(const char * data, int len)
	{
#ifdef WIN32
		std::lock_guard<std::mutex> lck(mtx_buffer_);
#endif // WIN32

		if (buffer_.size() > MAX_BUFFER_SIZE)
		{
			std::ostringstream oss;
			oss << "Too big buffer size:[" << buffer_.size() << "]";
			stdxLogWarn(oss.str());
		}

		for (auto i(0); i < len; ++i)
		{
			buffer_.push_back(data[i]);
		}

		return true;
	}

    std::shared_ptr<Packet> TcpLinker::get_packet()
    {
#ifdef WIN32
		std::lock_guard<std::mutex> lck(mtx_buffer_);
#endif // WIN32

        return unserialize(buffer_);
    }

}}