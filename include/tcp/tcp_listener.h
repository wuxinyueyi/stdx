
/*
A IOCP or Epoll based tcp listner.
on linux, each listen thread has it's own resources, so it is a lockless,high performance implementation
on windows, theads are managed by the OS. 
*/
#ifndef __STDX_TCP_TCPLISTENER_H_
#define __STDX_TCP_TCPLISTENER_H_

#include <memory>
#include <functional>
#include <vector>

using namespace std;

namespace stdx {namespace tcp {
	class Impl;
	class TcpLinker;
	struct Packet;
	struct PacketHead;


class TcpListener {
public:
	//if read_thread_count = 0 the read thread will be set to std::thread::hardware_concurrency()
	TcpListener(unsigned int port, unsigned int read_thread_count = 0);
	virtual ~TcpListener();
	
public:
	//start tcp listener
	bool start();
	void stop();
	bool is_stop();

	//methods those send plain data
	int send_by_fd(int fd, const char* data, unsigned int data_len);
	int send_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len);
	int broadcast_data(const char* data, unsigned int data_len);

	//methods,make a Packet struct stream,then send the stream data
	int send_packet_data_by_fd(int fd, const char* data, unsigned int len, const std::shared_ptr<PacketHead>& head_ptr);
	int send_packet_data_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr);
	int broadcast_packet_data(const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr);

	//get a copy of linkers_ vector
	std::vector<shared_ptr<TcpLinker>> get_linkers();

	//set callbacks
	void set_on_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn);
	void set_on_close(const std::function<void(int fd, unsigned short port, string ip)>& fn);
	void set_on_data(const std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)>& fn);

	//set connection timeout value,defualt is 30000 milliseconds
	//if no data comes more than tm_milliseconds milliseconds,the socket will be closed.
	void set_conn_timeout(unsigned int tm_milliseconds) ;

	//return connection status,shows how many conenctions and how many tasks
	//only works on Linux
	std::string status();

private:
	std::unique_ptr<Impl> impl_;

	
};
}}
#endif //__STDX_TCP_TCPLISTENER_H_
