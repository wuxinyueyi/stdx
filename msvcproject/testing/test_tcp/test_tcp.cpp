// test_tcp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//


#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <condition_variable>
#include <mutex>
#include <vector>
#include <map>
#include <thread>
#include <iostream>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"kernel32.lib")

std::condition_variable cond;
std::mutex con_mutx;

#define BUFFER_SIZE 256
int thread_cnt(1);

enum EventType
{
	kNULL = 0,
	kAccept,
	kRead,
	kWrite,
	kExit,//exit iocp queue
};

struct IOContext
{
	WSAOVERLAPPED overlapped_;	
	std::vector<char> buffer_;
	WSABUF wsabuf_;
	EventType type_;
	SOCKET socket_;

	IOContext(int buffer_size=BUFFER_SIZE)
	{
		buffer_.resize(buffer_size);
		reset();
	}

	IOContext(SOCKET socket, EventType type, int buffer_size = BUFFER_SIZE)
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

struct CompletionKey
{
	SOCKET socket_;
	EventType type_;
	CompletionKey() :socket_(-1), type_(kNULL) {}
};

LPFN_ACCEPTEX gFnAcceptEx;
LPFN_GETACCEPTEXSOCKADDRS gFnGetAddr;

CompletionKey *svr_key(NULL);
std::map<int, CompletionKey*> clts;

HANDLE handle_iocp;

bool getFunctions(int listen_fd)
{
	DWORD bytes(0);
	GUID guid_accept_ex = WSAID_ACCEPTEX;
	if (0 != WSAIoctl(listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_accept_ex, sizeof(guid_accept_ex), &gFnAcceptEx, sizeof(gFnAcceptEx), &bytes, NULL, NULL))
	{
		return false;
	}

	GUID guid_getaddr = WSAID_GETACCEPTEXSOCKADDRS;
	bytes = 0;
	if (0 != WSAIoctl(listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid_getaddr, sizeof(guid_getaddr), &gFnGetAddr, sizeof(gFnGetAddr), &bytes, NULL, NULL))
	{
		return false;
	}

	return true;
}

bool post_accept(SOCKET listen_socket)
{
	SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_HANDLE_VALUE == (HANDLE)socket)
	{
		return false;
	}
	int reuse_addr = 1;
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr));

	IOContext *ctx = new IOContext(socket, kAccept);	

	DWORD bytes_recv(0);

	if (FALSE == gFnAcceptEx(listen_socket, ctx->socket_, ctx->wsabuf_.buf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes_recv, &ctx->overlapped_))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return false;
		}
	}

	return true;
}

bool post_recv(SOCKET socket,int buffer_size=BUFFER_SIZE)
{
	IOContext* ctx = new IOContext(socket, kRead, buffer_size);

	DWORD r(0);
	DWORD flg(0);
	if (SOCKET_ERROR == WSARecv(ctx->socket_, &ctx->wsabuf_, 1, &r, &flg, &(ctx->overlapped_), NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return false;
		}

		return true;
	}

	return false;
}

bool post_send(SOCKET socket,const char* data, int len)
{
	IOContext *ctx = new IOContext(socket,kWrite,len);
	memcpy(ctx->wsabuf_.buf, data, len);	

	DWORD send(0);
	if (SOCKET_ERROR == WSASend(socket, &ctx->wsabuf_, 1, &send, 0, &ctx->overlapped_, NULL))
	{
		if (WSA_IO_PENDING != WSAGetLastError())
		{
			return false;
		}

		return true;
	}

	return false;
}

void thread_fn()
{
	CompletionKey *key(NULL);
	WSAOVERLAPPED* overlapped_p(NULL);
	DWORD bytes(0);

	while (true)
	{
		BOOL ret = GetQueuedCompletionStatus(handle_iocp, &bytes, (PULONG_PTR)&key, &overlapped_p, WSA_INFINITE);
		if (FALSE == ret)
		{
			//handle error;			
			std::cout << "get iocp failed:" << WSAGetLastError() << std::endl;
			continue;
		}

		std::cout << " get iocp from fd" << key->socket_ << std::endl;

//1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111122222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333333334444
		IOContext* ctx = CONTAINING_RECORD(overlapped_p, IOContext, overlapped_);

		if (NULL == ctx)
		{
			std::cerr << "got Null context" << std::endl;
			continue;
		}

		//connection closed;
		if (0 == bytes && (kRead == ctx->type_ || kWrite == ctx->type_))
		{
			std::cout << "connection closed:" << key->socket_ << std::endl;
			//todo::delete
			closesocket(key->socket_);			
			clts.erase(key->socket_);
			delete key;

			delete ctx;
			
			continue;
		}

		if (ctx->type_ == kAccept)
		{
			int len = sizeof(SOCKADDR_IN) + 16;

			SOCKADDR_IN* local_addr_p(NULL);
			SOCKADDR_IN* remote_addr_p(NULL);

			int local_len = sizeof(SOCKADDR_IN);
			int remote_len = sizeof(SOCKADDR_IN);

			gFnGetAddr(ctx->wsabuf_.buf, 0, len, len, (LPSOCKADDR*)&local_addr_p, &local_len, (LPSOCKADDR*)&remote_addr_p, &remote_len);
			if (NULL == local_addr_p || NULL == remote_addr_p)
			{
				std::cerr << "acceptex failed" << std::endl;
				delete ctx;
				continue;
			}

			std::cout << "New connection from " << inet_ntoa(remote_addr_p->sin_addr) << ":" << ntohs(remote_addr_p->sin_port) << ", fd:" << ctx->socket_ << std::endl;

			CompletionKey *clt_key = new CompletionKey();
			clt_key->socket_ = ctx->socket_;						
			clts[ctx->socket_] = clt_key;
			
			HANDLE h = CreateIoCompletionPort((HANDLE)clt_key->socket_, handle_iocp, (DWORD)clt_key, 0);
			if (h != handle_iocp)
			{
				//add to iocp failed
				std::cerr << "add socket " << clt_key->socket_ << " to iocp failed" << std::endl;
				delete ctx;
				continue;
			}

			post_recv(clt_key->socket_);

			//send welcome ack

			std::string ack("welcome to my iocp demo project \n");
			post_send(clt_key->socket_, ack.data(), ack.size());

			//post a new connection
			post_accept(key->socket_);

			delete ctx;
			continue;
		}		

		if (ctx->type_ == kRead)
		{
			std::string recvd(ctx->wsabuf_.buf, ctx->overlapped_.InternalHigh);

			std::cout << "recved:[" << recvd.c_str() << "] from" << key->socket_ << std::endl;
			//todo get data
			
			post_recv(ctx->socket_);

			//send back
			post_send(ctx->socket_, recvd.data(), recvd.size());

			delete ctx;

			continue;
		}

		if (ctx->type_ == kWrite)
		{
			if (ctx->overlapped_.InternalHigh < ctx->wsabuf_.len) //not totally send. not tested
			{
				post_send(ctx->socket_, ctx->wsabuf_.buf + ctx->overlapped_.InternalHigh, ctx->wsabuf_.len - ctx->overlapped_.InternalHigh);
			}

			delete ctx;

			continue;
		}

		if (ctx->type_ == kExit)
		{
			
			delete ctx;

			std::cout << "exit thread" << std::endl;
			break;
		}

		delete ctx;
	}
}

void stop()
{
	if (NULL != svr_key)
	{
		if (INVALID_HANDLE_VALUE != (HANDLE)svr_key->socket_)
		{
			closesocket(svr_key->socket_);
		}
	}

	for (auto c : clts)
	{
		closesocket(c.first);
	}

	while (!clts.empty())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}


	for (int i(0); i < thread_cnt; ++i)
	{
		IOContext* ctx = new IOContext(-1, kExit, 0);
		PostQueuedCompletionStatus(handle_iocp, 0, (ULONG_PTR)ctx, &ctx->overlapped_);
	}

	CloseHandle(handle_iocp);

	WSACleanup();
}

BOOL WINAPI control_handler_route(DWORD controlType)
{
	if (controlType == CTRL_LOGOFF_EVENT ||
		controlType == CTRL_CLOSE_EVENT ||
		controlType == CTRL_SHUTDOWN_EVENT ||
		controlType == CTRL_C_EVENT)
	{
		stop();
		cond.notify_all();
		return TRUE;
	}
	return FALSE;
}

int main1()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	bool ok(false);

	SetConsoleCtrlHandler(control_handler_route, TRUE);

	do
	{
		handle_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (INVALID_HANDLE_VALUE == handle_iocp)
		{
			break;
		}

		SOCKET listen_fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_HANDLE_VALUE == (HANDLE)listen_fd)
		{
			break;
		}

		std::cout << "create listen fd:" << listen_fd << std::endl;

		svr_key = new CompletionKey();
		svr_key->socket_ = listen_fd;
		HANDLE h = CreateIoCompletionPort((HANDLE)listen_fd, handle_iocp, (DWORD)svr_key, 0);
		if (h != handle_iocp)
		{
			break;
		}

		struct sockaddr_in svr_addr;
		memset(&svr_addr, 0, sizeof(svr_addr));
		svr_addr.sin_addr.S_un.S_addr = INADDR_ANY;
		svr_addr.sin_family = AF_INET;
		svr_addr.sin_port = htons(12345);

		if (0 != bind(listen_fd, (sockaddr*)&svr_addr, sizeof(svr_addr)))
		{
			//WSAGetLastError()
			break;
		}

		if (0 != listen(listen_fd, SOMAXCONN))
		{
			//WSAGetLastError()
			break;
		}

		if (!getFunctions(listen_fd))
		{
			break;
		}

		thread_cnt = std::thread::hardware_concurrency();

		for (int i(0); i < thread_cnt; ++i)
		{
			std::thread trd = std::thread(thread_fn);
			trd.detach();
		}

		std::cout << "start " << thread_cnt << " threads" << std::endl;

		for (int i(0); i < 128; ++i)
		{
			if (!post_accept(listen_fd))
			{
				break;
			}
		}

		ok = true;
	} while (0);

	
	if (ok)
	{
		std::unique_lock<std::mutex> lck(con_mutx);
		cond.wait(lck);
	}
	else
	{
		stop();
	}

	std::cout << "stoped" << std::endl;

	return -1;
}

