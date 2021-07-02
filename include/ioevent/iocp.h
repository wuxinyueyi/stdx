/*
this is a windows IOCP wrapper
*/
#ifndef __STDX_IO_EVENT_IOCP_H__
#define __STDX_IO_EVENT_IOCP_H__

#ifdef WIN32


#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <atomic>
#include <vector>
#include <map>
#include <thread>
#include <mutex>

namespace stdx {namespace ioevent {


class  IOCP
{
public:
	IOCP();
	virtual ~IOCP();

	//if is a server spceify a listen_fd
	//on client mode,listen_fd is set to -1
	bool start(int listen_fd, int thread_cnt=0);
	void stop();

	//add socket to iocp queue,listen means if the fd is a listening socket
	int add_fd(int fd,bool listen = false);
	//remove socket from iocp queue
	int del_fd(int fd);

	//server mode callbacks
	void set_callback(std::function<void(int, const std::string&, unsigned short)> on_accept, std::function<void(int)> on_close, std::function<void(int, const char*, int)> on_data)
	{
		fn_on_accept_ = on_accept;
		fn_on_close_ = on_close;
		fn_on_data_ = on_data;
	}

	//client mode callbacks,on client mode,the iocp object can be shared by several clients.each client has uniq callbakcs.
	void set_clt_callback(int fd, std::function<void(int)> on_conn, std::function<void(int)> on_close, std::function<void(int, const char*, int)> on_data);
	
	bool send_data(int fd, const char* data, int len)
	{
		if (fd < 0)
		{
			return false;
		}

		return post_send(fd, data, len);
	}

	//post connect request to iocp.
	int conenct_to(int fd, struct sockaddr_in* addr);
	
private:
	struct EventCallbacks
	{
		//callbacks
		std::function<void(int fd)> fn_on_conn_;
		std::function<void(int fd)> fn_on_close_;
		std::function<void(int fd, const char* data, int len)> fn_on_data_;

		EventCallbacks(std::function<void(int)> a, const std::function<void(int)> c, std::function<void(int, const char*, int)> d)
			:fn_on_conn_(a), fn_on_close_(c), fn_on_data_(d)
		{
		};
	};

	enum EventType
	{
		kNULL = 0,
		kAccept,
		kRead,
		kWrite,
		kConn,//connect
	};

	struct IOContext
	{	
		WSAOVERLAPPED overlapped_;
		std::vector<char> buffer_;
		WSABUF wsabuf_;
		EventType type_;
		int socket_;

		
		IOContext(int buffer_size = 512)
		{
			buffer_.resize(buffer_size);
			reset();
		}

		IOContext(int socket, EventType type, int buffer_size = 512)
			:socket_(socket), type_(type)
		{
			memset(&overlapped_, 0, sizeof(overlapped_));
			buffer_.resize(buffer_size);
			memset(buffer_.data(), 0, buffer_.size());

			wsabuf_.len = buffer_.size();
			wsabuf_.buf = buffer_.data();
		}

		void reset()
		{
			memset(&overlapped_, 0, sizeof(overlapped_));
			memset(buffer_.data(), 0, buffer_.size());

			wsabuf_.len = buffer_.size();
			wsabuf_.buf = buffer_.data();
			type_ = kNULL;
			socket_ = -1;
		}
	};

	struct Connection
	{
		SOCKET socket_;
		int type_;
		Connection() :socket_(-1), type_(kNULL) {}
	};
private:
	void thread_fn();
	bool get_functions();
	bool post_accept();
	bool post_recv(int fd, int buffer_size = 4 * 1024);
	bool post_send(int fd, const char* data, int len);
	std::shared_ptr<IOContext> get_ctx(int socket, EventType type, int buffer_size = 512);
	bool del_ctx(IOContext* ctx);
	std::shared_ptr<Connection> get_conn(int socket);
	bool del_conn(int socket);
	void iocp_close(int fd);
	void iocp_accept(IOContext *ctx);
	void iocp_read(IOContext *ctx);
private:
	int iocp_handle_;
	int listen_fd_;//listen socket
	std::atomic<bool> stop_;
	int thread_cnt_;

	std::vector<std::thread> threads_;
	std::map<int, std::shared_ptr<Connection>> conns_;
	std::mutex mtx_conn_;

	LPFN_ACCEPTEX fn_acceptex_;
	LPFN_GETACCEPTEXSOCKADDRS fn_get_addr_;
	LPFN_CONNECTEX fn_connex_;

	//server mode callback
	std::function<void(int fd, const std::string& ip, unsigned short port)> fn_on_accept_;
	std::function<void(int fd)> fn_on_close_;
	std::function<void(int fd, const char* data, int len)> fn_on_data_;

	//client mode callbacks
	std::map<int, EventCallbacks> callbacks_;
	std::mutex mtx_callback_;

	std::map<IOContext*, std::shared_ptr<IOContext>> contexts_;	
	std::mutex mtx_ctx_;

	
};
}}

#endif // WIN32

#endif // !__STDX_IO_EVENT_IOCP_H__

