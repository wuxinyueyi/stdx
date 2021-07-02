#include "tcp/tcp_client.h"
#include "tcp/packet.h"

#include "stdx.h"
#include <sstream>
#include <condition_variable>

#ifdef WIN32
#include "ioevent/iocp.h"
#else
#include "ioevent/io_event.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#endif // WIN32

namespace stdx {namespace tcp {
#ifdef WIN32

	class Impl
	{
	public:
		Impl():fd_(-1), started_(false), stoped_(false), shared_iocp_(true), connected_(false), connecting_(false)
		{
			WSADATA wsaData;
			WSAStartup(MAKEWORD(2, 2), &wsaData);
		};
		virtual~Impl()
		{
			stop();
			WSACleanup();
		};
		bool start()
		{
			shared_iocp_ = false;
			return start(std::make_shared<ioevent::IOCP>());
		};
		//pass evt_ptr in case of multi client share on ioevent
		bool start(const std::shared_ptr<stdx::ioevent::IOCP>& iocp_ptr)
		{
			if (started_)
			{
				return true;
			}

			do
			{
				fd_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				if (INVALID_HANDLE_VALUE == (HANDLE)fd_)
				{
					std::ostringstream oss;
					oss << "Create listen socket failed:" << WSAGetLastError();
					stdxLogError(oss.str());
					break;
				}

				iocp_ptr_ = iocp_ptr;
				if (!iocp_ptr_->start(-1, 0))
				{
					stdxLogError("start iocp failed");

					break;
				}

				iocp_ptr_->set_clt_callback(fd_, std::bind(&Impl::on_conn, this, std::placeholders::_1),
					std::bind(&Impl::on_close, this, std::placeholders::_1),
					std::bind(&Impl::on_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

				started_ = true;

			} while (false);

			if (!started_ && -1 != fd_)
			{
				closesocket(fd_);
				fd_ = -1;
			}

			return started_;
		};
		void stop() 
		{
			if (stoped_)
			{
				return;
			}

			if (!shared_iocp_)
			{
				iocp_ptr_->stop();
			}

			closesocket(fd_);
			started_ = false;
			connected_ = false;
			connecting_ = false;
			stoped_ = true;
		};
		bool connect_to(const std::string& host, unsigned short port, int timeout_seconds = 5) 
		{
			if (connected_ || connecting_)
			{
				stdxLogWarn("connect failed. alrady connected");
				return true;
			}

			connecting_ = true;
			connected_ = false;

			bool cache_host(false);
			do
			{
				if (-1 == fd_) //reconnect
				{
					fd_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					if (INVALID_HANDLE_VALUE == (HANDLE)fd_)
					{
						std::ostringstream oss;
						oss << "Create listen socket failed:" << WSAGetLastError();
						stdxLogError(oss.str());
						break;
					}

					if (iocp_ptr_)
					{
						iocp_ptr_->set_clt_callback(fd_, std::bind(&Impl::on_conn, this, std::placeholders::_1),
							std::bind(&Impl::on_close, this, std::placeholders::_1),
							std::bind(&Impl::on_data, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
					}
				}

				host_addr_.sin_family = AF_INET;
				host_addr_.sin_port = htons(port);

				unsigned long addr = inet_addr(host.c_str());

				if (0 == addr)//host is not a ip address
				{
					std::lock_guard<std::mutex> lck(Impl::mutx_);

					auto it = Impl::host_map_.find(host);
					if (it == Impl::host_map_.end())
					{
						struct hostent *hostent = gethostbyname(host.c_str());
						if (NULL == hostent)
						{
							std::ostringstream oss;
							oss << "gethostbyname failed, host[" << host << "] err:" << GetLastError();
							stdxLogError(oss.str());
							break;
						}

						host_addr_.sin_addr = *((struct in_addr*)(hostent->h_addr_list[0]));
						cache_host = true;
					}
					else
					{
						host_addr_ = it->second;
					}
				}
				else
				{
					host_addr_.sin_addr.S_un.S_addr = addr;
				}

				struct sockaddr_in clt_addr;
				clt_addr.sin_family = AF_INET;
				clt_addr.sin_addr.S_un.S_addr = INADDR_ANY;
				clt_addr.sin_port = htons(0);
				if (0 != ::bind(fd_, (sockaddr*)&clt_addr, sizeof(clt_addr)))
				{
					std::ostringstream oss;
					oss << "bind client socket failed:" << GetLastError();
					stdxLogError(oss.str());
					break;
				}

				int c = iocp_ptr_->conenct_to(fd_, &host_addr_);

				if (WSA_IO_PENDING == c)
				{
					std::unique_lock<std::mutex> lck(mutx_cond_);
					cond_.wait_for(lck, std::chrono::milliseconds(timeout_seconds));
				}
				else if (ERROR_SUCCESS == c)
				{
					connected_ = true;//directly connected
				}

			} while (false);

			connecting_ = false;

			if (connected_)
			{
				if (cache_host)
				{
					std::lock_guard<std::mutex> lck(stdx::tcp::Impl::mutx_);
					Impl::host_map_[host] = host_addr_;
				}
			}
			else
			{
				closesocket(fd_);
				fd_ = -1;

				std::unique_lock<std::mutex> lck1(mutx_cond_);
				cond_.wait_for(lck1, std::chrono::hours(timeout_seconds));

				std::lock_guard<std::mutex> lck(stdx::tcp::Impl::mutx_);
				Impl::host_map_.erase(host);
			}

			return connected_.load();
		};
		void disconnect() 
		{
			closesocket(fd_);
		};

		//directly send plain data to socekt
		int send_data(const char* data, unsigned int len) 
		{
			if (!connected_)
			{
				return -1;
			}

			return iocp_ptr_->send_data(fd_, data, len);
		};

		//make a Packet struct stream,then send the stream to the socket
		int send_packet_data(const char* data, int len, const std::shared_ptr<PacketHead>& head_ptr) 
		{
			std::vector<char> pkt_data = serialize(data, len, head_ptr);
			return send_data(pkt_data.data(), pkt_data.size());
		};

		//set callbacks
		inline void set_on_data(const std::function<void(int fd, std::shared_ptr<Packet> pkt_ptr)>& fn) { on_data_callback_ = fn; };
		inline void set_on_closed(const std::function<void(int fd)>& fn) { on_closed_callback_ = fn; };

		inline int get_fd() { return fd_; };
		bool is_connected() 
		{
			return connected_;
		};
	private:
		void on_conn(int fd) 
		{
			if (fd == fd_ && connecting_)
			{
				connected_ = true;
				cond_.notify_all();
			}
		};
		void on_close(int fd) 
		{
			if (on_closed_callback_)
			{
				on_closed_callback_(fd);
			}

			connected_ = false;
			fd_ = -1;
		};
		void on_data(int fd, const char* data, int len)
		{
			std::lock_guard<std::mutex> lck(mtx_buffer_);

			for (int i(0); i < len; ++i)
			{
				buffer_.push_back(data[i]);
			}

			auto pkt_ptr = unserialize(buffer_);
			while (nullptr != pkt_ptr)
			{
				if (on_data_callback_)
				{
					on_data_callback_(fd, pkt_ptr);
				}

				pkt_ptr = unserialize(buffer_);
			}
		};

	private:
		std::function<void(int fd, const std::shared_ptr<Packet> pkt_ptr)> on_data_callback_;
		std::function<void(int fd)> on_closed_callback_;
		int fd_;
		bool started_;
		bool stoped_;
		std::atomic<bool> connected_;
		std::atomic<bool> connecting_;
		
		std::shared_ptr<stdx::ioevent::IOCP> iocp_ptr_;
		bool shared_iocp_;
		struct sockaddr_in host_addr_;

		static std::mutex mutx_;
		static std::map<std::string, struct sockaddr_in> host_map_;

		std::mutex mutx_cond_;
		std::condition_variable cond_;

		std::vector<char> buffer_;
		std::mutex mtx_buffer_;
	};
#else
	class Impl
	{
	public:
		Impl() :fd_(-1), started_(false),stoped_(false),shared_ioevent_(false), connected_(false), connecting_(false)
		{

		};
		virtual ~Impl() { stop(); };
		bool start() 
		{
			if (started_)
			{
				return true;
			}

			fd_ = socket(AF_INET, SOCK_STREAM, 0);
			if (-1 == fd_)
			{
				std::ostringstream oss;
				oss << "create socket failed, err:" << strerror(errno);
				stdxLogError(oss.str());

				return false;
			}
			set_noblock();

			event_ptr_ = std::make_shared<stdx::ioevent::IOEvent>();

			if (!event_ptr_->start(nullptr, nullptr))
			{
				stdxLogError("start IO event failed");
				return false;
			}
			event_ptr_->add_fd(fd_, true, true, std::bind(&Impl::on_event_read, this, std::placeholders::_1), std::bind(&Impl::on_event_write, this, std::placeholders::_1));

			started_ = true;

			return true;
		};
		//pass evt_ptr in case of multi client share on ioevent
		bool start(const std::shared_ptr<stdx::ioevent::IOEvent>& evt_ptr) 
		{
			if (started_)
			{
				return true;
			}

			fd_ = socket(AF_INET, SOCK_STREAM, 0);
			if (-1 == fd_)
			{
				std::ostringstream oss;
				oss << "create socket failed, err:" << strerror(errno);
				stdxLogError(oss.str());
				return false;
			}

			set_noblock();

			if (nullptr != evt_ptr)
			{
				event_ptr_ = evt_ptr;
				shared_ioevent_ = true;

				if (!event_ptr_->start(nullptr, nullptr))
				{
					stdxLogError("start IO event failed");

					close(fd_);
					fd_ = -1;
					return false;
				}

				event_ptr_->add_fd(fd_, true, true, std::bind(&Impl::on_event_read, this, std::placeholders::_1), std::bind(&Impl::on_event_write, this, std::placeholders::_1));
			}

			started_ = true;

			return true;
		};
		void stop() 
		{
			if (stoped_)
			{
				return;
			}

			close_fd();

			if (!shared_ioevent_ && nullptr != event_ptr_)
			{
				event_ptr_->stop();
			}
			started_ = false;
			stoped_ = true;
		};
		bool connect_to(const std::string& host, unsigned short port, int timeout_seconds = 5)
		{
			if (connected_ || connecting_)
			{
				return true;
			}

			connecting_ = true;
			connected_ = false;

			bool cache_host(false);

			do
			{
				if (-1 == fd_) //reconnect
				{
					fd_ = socket(AF_INET, SOCK_STREAM, 0);
					if (-1 == fd_)
					{
						std::ostringstream oss;
						oss << "create socket failed, err:" << strerror(errno);
						stdxLogError(oss.str());
						break;
					}

					if (nullptr != event_ptr_)
					{
						event_ptr_->add_fd(fd_, true, true, std::bind(&Impl::on_event_read, this, std::placeholders::_1), std::bind(&Impl::on_event_write, this, std::placeholders::_1));
					}
				}

				host_addr_.sin_family = AF_INET;
				host_addr_.sin_port = htons(port);

				if (0 == inet_aton(host.c_str(), &host_addr_.sin_addr))//host is not a ip address
				{
					std::lock_guard<std::mutex> lck(Impl::mutx_);

					auto it = Impl::host_map_.find(host);
					if (it == Impl::host_map_.end())
					{
						struct hostent *hostent = gethostbyname(host.c_str());
						if (NULL == hostent)
						{
							std::ostringstream oss;
							oss << "gethostbyname failed, host[" << host << "] err:" << strerror(errno);
							stdxLogError(oss.str());
							break;
					}

						host_addr_.sin_addr = *((struct in_addr*)(hostent->h_addr_list[0]));
						cache_host = true;
				}
					else
					{
						host_addr_ = it->second;
					}
			}

				int ret = connect(fd_, (sockaddr*)&host_addr_, sizeof(host_addr_));
				if (ret != 0)
				{
					if (errno == EINPROGRESS)
					{
						std::unique_lock<std::mutex> lck(mutx_cond_);
						cond_.wait_for(lck, std::chrono::seconds(timeout_seconds));
					}
				}
				else
				{
					connected_ = true;
				}

		}while (false);

		connecting_ = false;

		if (connected_)
		{
			if (cache_host)
			{
				std::lock_guard<std::mutex> lck(stdx::tcp::Impl::mutx_);
				Impl::host_map_[host] = host_addr_;
			}
			if (nullptr != event_ptr_)
			{
				event_ptr_->mod_fd(fd_, true, false);
			}
		}
		else
		{
			close(fd_);

			std::lock_guard<std::mutex> lck(stdx::tcp::Impl::mutx_);
			Impl::host_map_.erase(host);
		}

		return connected_.load();
		};
		void disconnect()
		{
			close_fd();
		};

		//directly send plain data to socekt
		int send_data(const char* data, unsigned int len) 
		{
			int ret = send(fd_, data, len, 0);

			if (-1 == ret)
			{
				int e(errno);
				if (e == EAGAIN || e == EWOULDBLOCK || e == EALREADY || e == EINTR)
				{
					return 0;
				}
				else
				{
					close_fd();
				}

				std::ostringstream oss;
				oss << "send data failed, fd[" << fd_ << "] err:" << strerror(e);
				stdxLogWarn(oss.str());
			}

			return ret;
		};

		//make a Packet struct stream,then send the stream to the socket
		int send_packet_data(const char* data, int len, const std::shared_ptr<PacketHead>& head_ptr) 
		{
			std::vector<char> pkt_data = serialize(data, len, head_ptr);
			return send_data(pkt_data.data(), pkt_data.size());
		};

		//set callbacks
		inline void set_on_data(const std::function<void(int fd, std::shared_ptr<Packet> pkt_ptr)>& fn) { on_data_callback_ = fn; };
		inline void set_on_closed(const std::function<void(int fd)>& fn) { on_closed_callback_ = fn; };

		inline int get_fd() { return fd_; };
		inline bool is_connected() { return connected_.load(); };
	private:
		void on_event_read(int fd)
		{
			if (fd != fd_)
			{
				std::ostringstream oss;
				oss << "wrong event fd[" << fd << "] the expected fd is[" << fd_ << "]";
				stdxLogWarn(oss.str());
				return;
			}

			std::vector<char> buffer;

			int read_buffer_size(512);
			std::vector<char> read_buffer(read_buffer_size, '\0');

			buffer.reserve(read_buffer_size);

			int cnt(0);
			while (true)
			{
				cnt = read(fd, &(read_buffer[0]), read_buffer_size);
				if (cnt > 0)
				{
					buffer.insert(buffer.begin() + buffer.size(), read_buffer.begin(), read_buffer.begin() + cnt);

					if (cnt == read_buffer_size)
					{
						continue;
				}
					else
					{
						break;
					}
			}

				if (0 == cnt)
				{

					close_fd();

					break;
				}

				if (-1 == cnt)
				{
					int err(errno);
					if (EAGAIN == err  //buffer is empty, need next notification.
						|| EINTR == err) //interrupted
					{
						break;
					}
					else
					{
						close_fd();

						break;
					}
				}
		}

			buffer_.insert(buffer_.begin() + buffer_.size(), buffer.begin(), buffer.end());

			auto pkt_ptr = unserialize(buffer_);
			while (nullptr != pkt_ptr)
			{
				if (on_data_callback_)
				{
					on_data_callback_(fd, pkt_ptr);
				}

				pkt_ptr = unserialize(buffer_);

			}
		};
		void on_event_write(int fd)
		{
			if (fd == fd_ && connecting_)
			{
				connected_ = true;
				cond_.notify_all();
			}
		};

		void close_fd() 
		{
			if (-1 != fd_)
			{
				if (connected_)
				{
					if (on_closed_callback_)
					{
						on_closed_callback_(fd_);
					}

					if (nullptr != event_ptr_)
					{
						event_ptr_->del_fd(fd_);
					}
				}

				close(fd_);
				fd_ = -1;
				connected_ = false;
			}
		};
		void set_noblock() 
		{
			int flag = fcntl(fd_, F_GETFL, 0);
			if (flag < 0)
			{
				return;
			}

			fcntl(fd_, F_SETFL, flag | O_NONBLOCK);
		};
	private:
		int fd_;
		bool started_;
		bool stoped_;
		struct sockaddr_in host_addr_;
		std::shared_ptr<stdx::ioevent::IOEvent> event_ptr_;
		bool shared_ioevent_;

		std::function<void(int fd, const std::shared_ptr<Packet> pkt_ptr)> on_data_callback_;
		std::function<void(int fd)> on_closed_callback_;

		std::atomic<bool> connected_;
		std::atomic<bool> connecting_;

		static std::mutex mutx_;
		static std::map<std::string, struct sockaddr_in> host_map_;

		std::mutex mutx_cond_;
		std::condition_variable cond_;

		std::vector<char> buffer_;
	};
#endif // WIN32

	
std::mutex Impl::mutx_;
std::map<std::string, struct sockaddr_in> Impl::host_map_;

	TcpClient::TcpClient()
	{
		impl_.reset(new Impl());
	}

	TcpClient::~TcpClient()
	{
		impl_.reset();
	}

	bool TcpClient::start()
	{
		return impl_->start();
	}

#ifdef WIN32
	bool TcpClient::start(const std::shared_ptr<stdx::ioevent::IOCP>& evt_ptr)
	{
		return impl_->start(evt_ptr);
	}
#else
	bool TcpClient::start(const std::shared_ptr<stdx::ioevent::IOEvent>& evt_ptr)
	{
		return impl_->start(evt_ptr);
	}
#endif // WIN32

	void TcpClient::stop()
	{
		impl_->stop();
	}

	bool TcpClient::connect_to(const std::string & ip, unsigned short port, int timeout_seconds)
	{
		return impl_->connect_to(ip, port, timeout_seconds);
	}

	void TcpClient::disconnect()
	{
		impl_->disconnect();
	}

	int TcpClient::send_data(const char * data, unsigned int len)
	{
		return impl_->send_data(data, len);
	}

	int TcpClient::send_packet_data(const char * data, int len, const std::shared_ptr<PacketHead>& head_ptr)
	{
		return impl_->send_packet_data(data, len, head_ptr);
	}

	void TcpClient::set_on_data(const std::function<void(int fd, std::shared_ptr<Packet>pkt_ptr)>& fn)
	{
		impl_->set_on_data(fn);
	}

	void TcpClient::set_on_closed(const std::function<void(int fd)>& fn)
	{
		impl_->set_on_closed(fn);
	}

	int TcpClient::get_fd()
	{
		return impl_->get_fd();
	}

	bool TcpClient::is_connected()
	{
		return impl_->is_connected();
	}
}}//end namespace