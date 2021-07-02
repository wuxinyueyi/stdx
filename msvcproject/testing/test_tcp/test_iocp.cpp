#include "iocp.h"
#include <iostream>

std::condition_variable cond;
std::mutex con_mutx;

std::shared_ptr<stdx::ioevent::IOCP> iocp_ptr;
SOCKET listen_fd;

void stop()
{
	iocp_ptr->stop();

	closesocket(listen_fd);
	WSACleanup();
}

BOOL WINAPI control_handle(DWORD controlType)
{
	if (controlType == CTRL_LOGOFF_EVENT ||
		controlType == CTRL_CLOSE_EVENT ||
		controlType == CTRL_SHUTDOWN_EVENT ||
		controlType == CTRL_C_EVENT)
	{
		
		cond.notify_all();
		return TRUE;
	}
	return FALSE;
}



void on_accept(int socket, const::std::string& ip, unsigned short port)
{
	std::cout << "accept from " << ip.c_str() << ":" << port << " socket" << socket << std::endl;
}

void on_close(int socket)
{
	std::cout << "socket closed:" << socket << std::endl;
}

void on_data(int socket, const char* data, int len)
{
	std::string str(data, len);

	std::cout << "on data:[" << str.c_str() << "] from:" << socket << std::endl;

	if (str == "stop")
	{
		stop();
	}
}

int main()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	bool ok(false);

	SetConsoleCtrlHandler(control_handle, TRUE);

	do
	{
		listen_fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_HANDLE_VALUE == (HANDLE)listen_fd)
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


		iocp_ptr = std::make_shared<stdx::ioevent::IOCP>();
		if (!iocp_ptr->start(listen_fd))
		{
			break;
		}

		iocp_ptr->set_callback(on_accept, on_close, on_data);

		ok = true;
	} while (0);


	if (ok)
	{
		std::unique_lock<std::mutex> lck(con_mutx);
		cond.wait(lck);
	}
	
	stop();
	
	std::cout << "stoped" << std::endl;

	return 0;
}