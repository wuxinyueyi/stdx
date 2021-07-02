/*
a simple tcp client base on Windos IOCP or Linux epoll
*/

#ifndef __STDX_TCP_CLIENT_H__
#define _STDX_TCP_CLIENT_H__

#include <memory>
#include <string>
#include <functional>

namespace stdx {namespace ioevent {
	class IOCP;
	class IOEvent;
}}


namespace stdx {namespace tcp {
	
	struct PacketHead;
	struct Packet;

	class Impl;

	class TcpClient
	{
	public:
		TcpClient();
		virtual~TcpClient();
		bool start();
		
		//pass evt_ptr in case of multi client share on ioevent
#ifdef WIN32
		bool start(const std::shared_ptr<stdx::ioevent::IOCP>& evt_ptr);
#else
		bool start(const std::shared_ptr<stdx::ioevent::IOEvent>& evt_ptr);
#endif // WIN32

		
		void stop();
		bool connect_to(const std::string& ip, unsigned short port, int timeout_seconds = 5);
		void disconnect();

		//directly send plain data to socekt
		int send_data(const char* data, unsigned int len);

		//make a Packet struct stream,then send the stream to the socket
		int send_packet_data(const char* data, int len, const std::shared_ptr<PacketHead>& head_ptr);

		//set callbacks
		void set_on_data(const std::function<void(int fd, std::shared_ptr<Packet> pkt_ptr)>& fn);
		void set_on_closed(const std::function<void(int fd)>& fn);

		int get_fd();
		bool is_connected();

	private:
		std::unique_ptr<stdx::tcp::Impl> impl_;
	};
}}


#endif // !__STDX_TCP_CLIENT_H__

