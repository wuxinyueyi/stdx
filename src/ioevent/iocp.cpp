#ifdef WIN32

#include "ioevent/iocp.h"
#include "stdx.h"

#include <Windows.h>

#include <sstream>
#include <iostream>
#include <thread>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"kernel32.lib")


namespace stdx {namespace ioevent {

	IOCP::IOCP():iocp_handle_(-1), listen_fd_(-1),stop_(false),fn_acceptex_(NULL),fn_get_addr_(NULL),fn_connex_(NULL)
	{
		thread_cnt_ = std::thread::hardware_concurrency();
		if (thread_cnt_ < 1)
		{
			thread_cnt_ = 1;
		}
	}

	IOCP::~IOCP()
	{
		stop();
	}

	bool IOCP::start(int listen_fd, int thread_cnt)
	{
		if (-1 != iocp_handle_)
		{
			return true;
		}		

		iocp_handle_ = (int)CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (-1 == iocp_handle_)
		{
			std::ostringstream oss;
			oss << "add epoll event failed, err:" << GetLastError();
			stdxLogWarn(oss.str());

			return false;
		}

		if (0 != thread_cnt)
		{
			thread_cnt_ = thread_cnt;
		}

		if (listen_fd > 0)
		{
			add_fd(listen_fd, true);

			for (auto i(0); i < thread_cnt_ * 4; ++i)
			{
				post_accept();
			}
		}
		

		for (auto i(0); i < thread_cnt_; ++i)
		{
			threads_.emplace_back(std::thread(&IOCP::thread_fn, this));
			threads_[i].detach();
		}

		return true;
	}

	void IOCP::stop()
	{
		if (stop_)
		{
			return;
		}

		std::ostringstream oss;
		oss << "stop, key count:" << conns_.size() << " context count:" << contexts_.size() << std::endl;
		stdxLogWarn(oss.str());

		stop_ = true;

		mtx_conn_.lock();
		for (auto k : conns_)
		{	
			closesocket(k.first);
		}		
		mtx_conn_.unlock();

		int cnt(0);
		while (!conns_.empty())
		{
			if (cnt > 10)
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			++cnt;
		}

		conns_.clear();

		CloseHandle((HANDLE)iocp_handle_);
		iocp_handle_ = -1;

		mtx_ctx_.lock();
		contexts_.clear();
		mtx_ctx_.unlock();
	}

	int IOCP::add_fd(int fd, bool listen )
	{
		auto key_ptr = get_conn(fd);
		
		int h = (int)CreateIoCompletionPort((HANDLE)fd, (HANDLE)iocp_handle_, (DWORD)key_ptr.get(), 0);
		if (h != iocp_handle_)
		{
			std::ostringstream oss;
			oss << "add socket:[ " << fd << "] to iocp failed,err:" << GetLastError();
			stdxLogWarn(oss.str());		

			del_conn(fd);

			return -1;
		}

		if (listen)
		{
			listen_fd_ = fd;
			get_functions();
		}

		return 0;
	}

	int IOCP::del_fd(int fd)
	{
		del_conn(fd);

		return 0;
	}

	void IOCP::set_clt_callback(int fd, std::function<void(int)> on_conn, std::function<void(int)> on_close, std::function<void(int, const char*, int)> on_data)
	{	
		std::lock_guard<std::mutex> lck(mtx_callback_);
		callbacks_.emplace(fd, EventCallbacks(on_conn, on_close, on_data));
	}

	int IOCP::conenct_to(int fd, sockaddr_in * addr)
	{	
		LPFN_CONNECTEX fn_connex;
		GUID guid_connex = WSAID_CONNECTEX;
		DWORD bytes = 0;
		if (0 != WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_connex, sizeof(guid_connex), &fn_connex, sizeof(fn_connex), &bytes, NULL, NULL))
		{
			std::ostringstream oss;
			oss << "get connect ex socket address failed:" << WSAGetLastError();
			stdxLogWarn(oss.str());
			return SOCKET_ERROR;
		}
		
		auto ctx_ptr = get_ctx(fd, kConn, 128);
		add_fd(fd);

		DWORD bytes_sent(0);
		if (FALSE == fn_connex(fd, (sockaddr*)addr, sizeof(sockaddr_in), NULL, 0, &bytes_sent, &ctx_ptr->overlapped_))
		{
			int err = WSAGetLastError();
			if (WSA_IO_PENDING != err)
			{
				std::ostringstream oss;
				oss << "acceptex failed:" << err;
				stdxLogWarn(oss.str());

				del_ctx(ctx_ptr.get());
				del_fd(fd);
				return SOCKET_ERROR;
			}
			
			return WSA_IO_PENDING;
		}		

		return ERROR_SUCCESS;
	
	}


	void IOCP::thread_fn()
	{
		Connection *key_p(NULL);
		WSAOVERLAPPED* overlapped_p(NULL);
		DWORD bytes(0);
		IOContext *ctx_p(NULL);

		while (!stop_)
		{
			do
			{
				BOOL ret = GetQueuedCompletionStatus((HANDLE)iocp_handle_, &bytes, (PULONG_PTR)&key_p, &overlapped_p, WSA_INFINITE);
				
				ctx_p = CONTAINING_RECORD(overlapped_p, IOContext, overlapped_);
				
				if (FALSE == ret)
				{
					std::ostringstream oss;
					int err = GetLastError();
					oss << "get iocp failed:" << err;
					if (ctx_p)
					{
						oss << "fd:[" << ctx_p->socket_ << "]" << "oper:[" << ctx_p->type_ << "]" << std::endl;
					}
					stdxLogDebug(oss.str());

					//If overlapped_p is NULL, the function did not dequeue a completion packet from the completion port.
					if (NULL == overlapped_p)
					{											
						break;
					}

					if (NULL != key_p)
					{
						iocp_close(key_p->socket_);
					}

					if (NULL != ctx_p && key_p->socket_ != ctx_p->socket_)
					{
						iocp_close(ctx_p->socket_);
					}				

					break;
				}

				if (NULL == key_p || NULL == ctx_p)
				{
					break;
				}
				
				if (0 == bytes && (kRead == ctx_p->type_ || kWrite == ctx_p->type_))
				{	
					iocp_close(ctx_p->socket_);
					
					break;
				}

				if (ctx_p->type_ == kAccept)
				{
					iocp_accept(ctx_p);

					break;
				}

				if (ctx_p->type_ == kRead)
				{
					iocp_read(ctx_p);

					break;
				}

				if (ctx_p->type_ == kWrite)
				{
					if (ctx_p->overlapped_.InternalHigh < ctx_p->wsabuf_.len) //not totally send. not tested
					{
						post_send(ctx_p->socket_, ctx_p->wsabuf_.buf + ctx_p->overlapped_.InternalHigh, ctx_p->wsabuf_.len - ctx_p->overlapped_.InternalHigh);
					}

					break;
				}

				if (ctx_p->type_ == kConn)//client mode
				{
					std::lock_guard<std::mutex> lck(mtx_callback_);
					auto it = callbacks_.find(ctx_p->socket_);
					if (it == callbacks_.end())
					{
						stdxLogWarn("on connected error,no callbak");
						break;
					}

					it->second.fn_on_conn_(ctx_p->socket_);
										
					post_recv(ctx_p->socket_);
				}

				
			} while (false);

			if (NULL != ctx_p)
			{
				del_ctx(ctx_p);
			}
	
		}
	}

	bool IOCP::get_functions()
	{
		if (-1 == listen_fd_)
		{
			return false;
		}

		DWORD bytes(0);
		GUID guid_accept_ex = WSAID_ACCEPTEX;
		if (0 != WSAIoctl(listen_fd_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_accept_ex, sizeof(guid_accept_ex), &fn_acceptex_, sizeof(fn_acceptex_), &bytes, NULL, NULL))
		{
			std::ostringstream oss;
			oss << "get accept ex failed:" << WSAGetLastError();
			stdxLogWarn(oss.str());
			return false;
		}

		GUID guid_getaddr = WSAID_GETACCEPTEXSOCKADDRS;
		bytes = 0;
		if (0 != WSAIoctl(listen_fd_, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_getaddr, sizeof(guid_getaddr), &fn_get_addr_, sizeof(fn_get_addr_), &bytes, NULL, NULL))
		{
			std::ostringstream oss;
			oss << "get accept ex socket address failed:" << WSAGetLastError();
			stdxLogWarn(oss.str());
			return false;
		}

		return true;
	}

	bool IOCP::post_accept()
	{
		if (NULL == fn_acceptex_)
		{
			return false;
		}

		SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_HANDLE_VALUE == (HANDLE)socket)
		{
			std::ostringstream oss;
			oss << "create socket failed:" << WSAGetLastError();
			stdxLogWarn(oss.str());
			return false;
		}
		int reuse_addr = 1;
		setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr));

		auto ctx_ptr = get_ctx(socket, kAccept, 128);

		DWORD bytes_recv(0);

		if (FALSE == fn_acceptex_(listen_fd_, ctx_ptr->socket_, ctx_ptr->wsabuf_.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes_recv, &ctx_ptr->overlapped_))
		{
			int err = WSAGetLastError();
			if (WSA_IO_PENDING != err)
			{
				std::ostringstream oss;
				oss << "acceptex failed:" << err;
				stdxLogWarn(oss.str());

				del_ctx(ctx_ptr.get());
				return false;
			}

			return true;
		}

		del_ctx(ctx_ptr.get());
		return false;
	}

	bool IOCP::post_recv(int socket, int buffer_size)
	{
		auto ctx_ptr = get_ctx(socket, kRead, buffer_size);

		DWORD r(0);
		DWORD flg(0);
		if (SOCKET_ERROR == WSARecv(ctx_ptr->socket_, &ctx_ptr->wsabuf_, 1, &r, &flg, &(ctx_ptr->overlapped_), NULL))
		{
			int err = WSAGetLastError();
			if (WSA_IO_PENDING != err)
			{
				std::ostringstream oss;
				oss << "post recv WSARecv failed:" << err;
				stdxLogWarn(oss.str());
				
				del_ctx(ctx_ptr.get());
				closesocket(socket);
				return false;
			}

		}
		
		return true;
	}


	bool IOCP::post_send(int socket, const char* data, int len)
	{
		auto ctx_ptr = get_ctx(socket, kWrite, len);
		memcpy(ctx_ptr->wsabuf_.buf, data, len);

		DWORD send(0);
		if (SOCKET_ERROR == WSASend(socket, &ctx_ptr->wsabuf_, 1, &send, 0, &ctx_ptr->overlapped_, NULL))
		{
			int err = WSAGetLastError();
			if (WSA_IO_PENDING != err)
			{
				std::ostringstream oss;
				oss << "post send WSASend failed:" << err;
				stdxLogWarn(oss.str());

				del_ctx(ctx_ptr.get());
				closesocket(socket);
				return false;
			}

		}
		return true;
	}

	std::shared_ptr<IOCP::IOContext> IOCP::get_ctx(int socket, IOCP::EventType type, int buffer_size)
	{
		if (socket < 0)
		{
			return nullptr;
		}

		std::unique_lock<std::mutex> lck(mtx_ctx_);
		std::shared_ptr<IOContext> ptr = std::make_shared<IOContext>(socket, type, buffer_size);
		if (ptr)
		{
			contexts_[ptr.get()] = ptr;
		}

		return ptr;
	}	

	bool IOCP::del_ctx(IOContext* ctx)
	{
		if (NULL == ctx)
		{
			return false;
		}

		std::unique_lock<std::mutex> lck(mtx_ctx_);
		
		contexts_.erase(ctx);		

		return 0;
	}

	std::shared_ptr<IOCP::Connection> IOCP::get_conn(int socket)
	{
		std::lock_guard<std::mutex> lck(mtx_conn_);
		auto ptr = std::make_shared<Connection>();
		ptr->socket_ = socket;
		ptr->type_ = kNULL;

		conns_[socket] = ptr;

		return ptr;
	}

	bool IOCP::del_conn(int socket)
	{		
		mtx_conn_.lock();
		conns_.erase(socket);
		mtx_conn_.unlock();
		if (-1 == listen_fd_) //client mode
		{
			std::lock_guard<std::mutex> lck(mtx_callback_);
			callbacks_.erase(socket);
		}
		return true;
	}

	void IOCP::iocp_close(int fd)
	{
		if (fn_on_close_ && -1 != listen_fd_)//server mode
		{
			fn_on_close_(fd);
		}
		else
		{
			std::lock_guard<std::mutex> lck(mtx_callback_);
			auto it = callbacks_.find(fd);
			if (it == callbacks_.end())
			{
				std::ostringstream oss;
				oss << "on close,no callbak,fd:[" << fd << "]";
				stdxLogWarn(oss.str());
				return;
			}
			it->second.fn_on_close_(fd);
		}

		closesocket(fd);
		del_fd(fd);
	}

	void IOCP::iocp_accept(IOContext *ctx)
	{
		if (NULL == ctx)
		{
			return;
		}

		int len = sizeof(SOCKADDR_IN) + 16;

		SOCKADDR_IN* local_addr_p(NULL);
		SOCKADDR_IN* remote_addr_p(NULL);

		int local_len = sizeof(SOCKADDR_IN);
		int remote_len = sizeof(SOCKADDR_IN);

		fn_get_addr_(ctx->wsabuf_.buf, 0, len, len, (LPSOCKADDR*)&local_addr_p, &local_len, (LPSOCKADDR*)&remote_addr_p, &remote_len);
		if (NULL == local_addr_p || NULL == remote_addr_p)
		{
			std::ostringstream oss;
			oss << "call acceptex socket address failed,err:" << WSAGetLastError();
			stdxLogWarn(oss.str());
			return;
		}

		if (fn_on_accept_)
		{
			fn_on_accept_(ctx->socket_, inet_ntoa(remote_addr_p->sin_addr), ntohs(remote_addr_p->sin_port));
		}

		add_fd(ctx->socket_);

		post_accept();
		post_recv(ctx->socket_);
	}

	void IOCP::iocp_read(IOContext *ctx)
	{
		if (NULL == ctx)
		{
			return;
		}

		if (fn_on_data_ && -1 != listen_fd_) //server mode
		{
			fn_on_data_(ctx->socket_, ctx->wsabuf_.buf, ctx->overlapped_.InternalHigh);
		}
		else
		{
			auto it = callbacks_.find(ctx->socket_);
			if (it == callbacks_.end())
			{
				stdxLogWarn("on read error,no callbak");
				return;
			}

			it->second.fn_on_data_(ctx->socket_, ctx->wsabuf_.buf, ctx->overlapped_.InternalHigh);
		}

		post_recv(ctx->socket_);
	}
}}

#endif // WIN32