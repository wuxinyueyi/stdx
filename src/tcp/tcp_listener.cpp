#include "tcp/tcp_listener.h"
#include "tcp/packet.h"
#include "tcp/tcp_linker.h"
#include "stdx.h"
#include "timer/timer.h"


#ifdef WIN32
#include "ioevent/iocp.h"
#else
#include "thread/task_thread_pool.h"
#include "ioevent/io_event.h"

#include <unordered_map>

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#endif // WIN32


namespace stdx {
	namespace tcp {
#ifdef WIN32
		class Impl
		{
		public:
			//if read_thread_count = 0 the read thread will be set to std::thread::hardware_concurrency()
			Impl(unsigned int port, unsigned int read_thread_count = 0) :port_(port), read_thread_cnt_(read_thread_count), stop_(false), conn_time_out_(30000)
			{
				if (read_thread_count > 0)
				{
					read_thread_cnt_ = read_thread_count;
				}
				else
				{
					read_thread_cnt_ = std::thread::hardware_concurrency();
				}

				WSADATA wsaData;
				WSAStartup(MAKEWORD(2, 2), &wsaData);
				iocp_ptr_.reset(new stdx::ioevent::IOCP());

				timer_ptr_ = make_shared<stdx::timer::Timer>();
			};

			virtual ~Impl()
			{
				stop();
				WSACleanup();
			};

		public:
			bool start()
			{
				if (listen_fd_ > 0)
				{
					return true;
				}

				bool ok(false);
				do
				{
					listen_fd_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
					if (INVALID_HANDLE_VALUE == (HANDLE)listen_fd_)
					{
						std::ostringstream oss;
						oss << "Create listen socket failed:" << WSAGetLastError();
						stdxLogError(oss.str());
						break;
					}

					struct sockaddr_in svr_addr;
					memset(&svr_addr, 0, sizeof(svr_addr));
					svr_addr.sin_addr.S_un.S_addr = INADDR_ANY;
					svr_addr.sin_family = AF_INET;
					svr_addr.sin_port = htons(port_);

					if (0 != ::bind(listen_fd_, (sockaddr*)&svr_addr, sizeof(svr_addr)))
					{
						std::ostringstream oss;
						oss << "bind listen socket failed:" << GetLastError();
						stdxLogError(oss.str());

						break;
					}

					if (0 != listen(listen_fd_, SOMAXCONN))
					{
						std::ostringstream oss;
						oss << "listen on socket failed:" << GetLastError();
						stdxLogError(oss.str());
						break;
					}

					if (!iocp_ptr_)
					{
						iocp_ptr_.reset(new stdx::ioevent::IOCP());
					}

					if (!iocp_ptr_->start(listen_fd_))
					{
						break;
					}

					iocp_ptr_->set_callback(std::bind(&Impl::on_accept, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
						std::bind(&Impl::on_close, this, std::placeholders::_1),
						std::bind(&Impl::on_date, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

					if (!timer_ptr_->start())
					{
						break;
					}
					timer_ptr_->add_timer(1, conn_time_out_, std::bind(&Impl::timer_check_fd, this, std::placeholders::_1, std::placeholders::_2));

					ok = true;
				} while (0);

				if (!ok)
				{
					if (listen_fd_ > 0)
					{
						closesocket(listen_fd_);
						listen_fd_ = 0;
					}

					iocp_ptr_->stop();

					timer_ptr_->stop();
				}

				return ok;
			};

			void stop()
			{
				iocp_ptr_->stop();
				timer_ptr_->stop();
				stop_ = true;
				linkers_.clear();
				listen_fd_ = -1;
			};

			bool is_stop()
			{
				return stop_;
			};

			//methods send plain data
			int send_by_fd(int fd, const char* data, unsigned int data_len)
			{
				return iocp_ptr_->send_data(fd, data, data_len);
			};

			int send_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len)
			{
				return iocp_ptr_->send_data(linker->fd(), data, data_len);
			};

			int broadcast_data(const char* data, unsigned int data_len)
			{
				auto linkers_cpy = linkers_;

				for (auto l : linkers_cpy)
				{
					if (l.first)
					{
						iocp_ptr_->send_data(l.first, data, data_len);
					}
				}

				return 0;
			};

			//make a Packet struct stream,then send the stream data
			int send_packet_data_by_fd(int fd, const char* data, unsigned int len, const std::shared_ptr<PacketHead>& head_ptr)
			{
				auto buffer = serialize(data, len, head_ptr);
				return send_by_fd(fd, buffer.data(), buffer.size());
			};

			int send_packet_data_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr)
			{
				auto buffer = serialize(data, data_len, head_ptr);
				return send_by_fd(linker->fd(), buffer.data(), buffer.size());
			};

			int broadcast_packet_data(const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr)
			{
				auto buffer = serialize(data, data_len, head_ptr);
				return broadcast_data(buffer.data(), buffer.size());
			};

			inline void set_on_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn) { accept_callback_ = fn; };
			inline void set_on_close(const std::function<void(int fd, unsigned short port, string ip)>& fn) { close_callback_ = fn; };
			inline void set_on_data(const std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)>& fn) { data_callback_ = fn; };

			//get a copy of linkers_ vectore
			vector<shared_ptr<TcpLinker>> get_linkers()
			{
				auto linkers_cpy = linkers_;
				vector<shared_ptr<TcpLinker>> ret_linkers;

				for (auto l : linkers_cpy)
				{
					if (l.second)
					{
						ret_linkers.push_back(l.second);
					}
				}

				return ret_linkers;
			};

			//set connection timeout value,defualt is 30000 milliseconds
			inline void set_conn_timeout(unsigned int tm_milliseconds) { conn_time_out_ = tm_milliseconds; };
			std::string status()
			{
				ostringstream oss;
				oss << "Linker count:" << linkers_.size() << std::endl;
				return oss.str();
			};

		private:
			void on_accept(int fd, const std::string& ip, unsigned int port)
			{
				mtx_linker_.lock();

				std::shared_ptr<TcpLinker> lptr = std::make_shared<TcpLinker>(fd, port, ip);
				linkers_[fd] = lptr;

				mtx_linker_.unlock();

				if (accept_callback_)
				{
					accept_callback_(fd, port, ip);
				}
			};

			void on_close(int fd)
			{
				if (close_callback_)
				{
					close_callback_(fd, 0, "");
				}
				std::lock_guard<std::mutex> lck(mtx_linker_);
				linkers_.erase(fd);
			};

			void on_date(int fd, const char* data, int len)
			{
				auto it = linkers_.find(fd);
				if (it == linkers_.end())
				{
					std::ostringstream oss;
					oss << "fd not found,fd[" << fd << "]";

					stdxLogError(oss.str());

					return;
				}

				it->second->update_alive_time();

				it->second->append_buffer(data, len);

				auto pkt_ptr = it->second->get_packet();
				int cnt(0);
				while (nullptr != pkt_ptr)
				{
					++cnt;
					if (data_callback_)
					{
						data_callback_(it->second->fd(), it->second->port(), it->second->ip(), pkt_ptr);
					}
					pkt_ptr = it->second->get_packet();
				}
			};
            
			void timer_check_fd(stdx::timer::Timer* timer, int id)
			{
				std::vector<int> v;
				for (auto it : linkers_)
				{
					if (std::chrono::system_clock::now() > it.second->last_alive_time())
					{
						int64_t d = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
							- std::chrono::duration_cast<std::chrono::milliseconds>(it.second->last_alive_time().time_since_epoch()).count();

						if (d >= conn_time_out_)
						{
							std::ostringstream oss;
							oss << "linker timeout,fd[" << it.first << "],remove it";
							stdxLogWarn(oss.str());

							v.push_back(it.first);
						}
					}
				}

				for (auto it : v)
				{
					closesocket(it);
				}
			};
		private:
			unsigned int port_;
			unsigned int read_thread_cnt_;
			int listen_fd_;

			std::unique_ptr<ioevent::IOCP> iocp_ptr_;

			std::atomic<bool> stop_;

			//callbacks
			std::function<void(int fd, unsigned short port, string ip)> accept_callback_;
			std::function<void(int fd, unsigned short port, string ip)> close_callback_;
			std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)> data_callback_;

			std::map<int, std::shared_ptr<TcpLinker>> linkers_;
			std::mutex mtx_linker_;

			//timer
			std::shared_ptr<stdx::timer::Timer> timer_ptr_;
			unsigned int conn_time_out_;
		};
#else
	class LinkerHandle
	{
	public:
		LinkerHandle(std::shared_ptr<stdx::ioevent::IOEvent>& event_ptr, int index = 0):event_ptr_(event_ptr)
		{
			trd_ = std::make_shared<stdx::thread::TaskThreadPool>();

			ostringstream oss;
			oss << "TcpListener thread pool " << index;

			trd_->set_name(oss.str());
		};

		virtual ~LinkerHandle() 
		{

		};

		void broadcast(const std::string& data, unsigned int data_len) 
		{
			for (auto l : linkers_)
			{
				if (-1 == send(l.first, data.data(), data_len, 0))
				{
					std::ostringstream oss;
					oss << "send to fd[" << l.first << "] failed,err:" << strerror(errno);
					stdxLogWarn(oss.str());
				}
			}
		};

		void task_accept(int listen_fd) 
		{
			struct sockaddr_in addr = {};
			socklen_t len = sizeof(addr);
			int conn_fd = accept(listen_fd, (sockaddr*)&addr, &len);

			while (conn_fd >= 0)
			{
				//dispatch to tis own thread
				if (dispatch_accept_callback_)
				{
					dispatch_accept_callback_(conn_fd, ntohs(addr.sin_port), inet_ntoa(addr.sin_addr));
				}

				memset(&addr, 0, len);
				conn_fd = accept(listen_fd, (sockaddr*)&addr, &len);

			}
		};

		void task_read(int fd) 
		{
			if (!update_linker_time(fd))
			{
				return;
			}

			int read_buffer_size(512);
			vector<char> read_buffer(read_buffer_size, '\0');

			vector<char> buffer;
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
					close_linker(fd);
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
						close_linker(fd);
						break;
					}
				}
			}


			auto it = linkers_.find(fd);
			if (it != linkers_.end() && it->second)
			{
				if (it->second->append_buffer(buffer))
				{
					auto pkt_ptr = it->second->get_packet();
					int cnt(0);
					while (nullptr != pkt_ptr)
					{
						++cnt;
						if (data_callback_)
						{
							data_callback_(it->second->fd(), it->second->port(), it->second->ip(), pkt_ptr);
						}
						pkt_ptr = it->second->get_packet();
					}
				}
			}

		};

		void task_check_fd(int timeout_milliseconds) 
		{
			std::vector<int> v;
			for (auto it : linkers_)
			{
				if (std::chrono::system_clock::now() > it.second->last_alive_time())
				{
					int64_t d = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
						- std::chrono::duration_cast<std::chrono::milliseconds>(it.second->last_alive_time().time_since_epoch()).count();

					if (d >= timeout_milliseconds)
					{
						std::ostringstream oss;
						oss << "linker timeout,fd[" << it.first << "],remove it";
						stdxLogWarn(oss.str());

						v.push_back(it.first);
					}
				}
			}

			for (auto it : v)
			{
				close_linker(it);
			}
		};

		void close_all() 
		{
			for (auto l : linkers_)
			{
				close_linker(l.first);
			}
		};

		void add_linker(int fd, unsigned short port, std::string ip) 
		{
			int ret(-1);
			auto linker_ptr = std::make_shared<TcpLinker>(fd, port, ip);

			ret = event_ptr_->add_fd(fd);
			if (0 == ret)
			{
				linkers_[fd] = linker_ptr;
			}

			if (0 == ret && accept_callback_)
			{
				accept_callback_(fd, port, ip);
			}
		};

		inline void set_on_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn) { accept_callback_ = fn; };
		inline void set_on_close(const std::function<void(int fd, unsigned short port, string ip)>& fn) { close_callback_ = fn; };
		inline void set_on_data(const std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)>& fn) { data_callback_ = fn; };
		inline void set_on_dispatch_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn) { dispatch_accept_callback_ = fn; };

	public:
		std::shared_ptr<stdx::thread::TaskThreadPool> trd_;
		std::unordered_map<int, shared_ptr<TcpLinker>> linkers_;

	private:
		void close_linker(int fd) 
		{
			std::string ip;
			unsigned short port(0);
			if (close_callback_)
			{
				auto it = linkers_.find(fd);
				if (it != linkers_.end())
				{
					ip = it->second->ip();
					port = it->second->port();
				}

			}

			event_ptr_->del_fd(fd);
			linkers_.erase(fd);

			close(fd);

			if (0 != port && close_callback_)
			{
				close_callback_(fd, port, ip);
			}
		};

		bool update_linker_time(int fd) 
		{
			auto it = linkers_.find(fd);
			if (it == linkers_.end())
			{
				return false;
			}

			if ((*it).second)
			{
				(*it).second->update_alive_time();
				return true;
			}

			return false;
		};

	private:
		std::shared_ptr<stdx::ioevent::IOEvent> event_ptr_;
		//callbacks
		std::function<void(int fd, unsigned short port, string ip)> accept_callback_;
		std::function<void(int fd, unsigned short port, string ip)> close_callback_;
		std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)> data_callback_;

		std::function<void(int fd, unsigned short port, string ip)> dispatch_accept_callback_;

	};
	
    class Impl
	{
	public:
		//if read_thread_count = 0 the read thread will be set to std::thread::hardware_concurrency()
		Impl(unsigned int port, unsigned int read_thread_count = 0) 
			:port_(port), listenfd_(-1), stop_(false), conn_time_out_(30000)
		{
			timer_ptr_ = make_shared<stdx::timer::Timer>();
			handle_cnt_ = std::thread::hardware_concurrency();
			if (read_thread_count != 0)
			{
				handle_cnt_ = read_thread_count;
			}

			event_ptr_ = make_shared<stdx::ioevent::IOEvent>();
			for (unsigned int i(0); i < handle_cnt_; ++i)
			{
				linker_handles_.emplace_back(std::make_shared<LinkerHandle>(event_ptr_, i));
			}
		};

		~Impl()
		{
			if (!stop_)
			{
				stop();
			}
		};
	public:
		bool start() 
		{
			if (listenfd_ != -1)//already started
			{
				return true;
			}

			bool ok(false);
			do
			{
				if (!create_listen_fd())
				{
					break;
				}

				if (!timer_ptr_->start())
				{
					break;
				}
				timer_ptr_->add_timer(1, conn_time_out_, std::bind(&Impl::timer_check_fd, this, std::placeholders::_1, std::placeholders::_2));

				for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
				{
					if (!linker_handles_[i]->trd_->start(1, 1))
					{
						stdxLogError("start handler thread failed");
						break;
					}
					linker_handles_[i]->set_on_dispatch_accept(std::bind(&Impl::dispatch_accept, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				}

				if (!event_ptr_->start(std::bind(&Impl::on_event_read, this, std::placeholders::_1), nullptr))
				{
					break;
				}
				event_ptr_->add_fd(listenfd_);

				ok = true;
			} while (false);


			if (!ok)
			{
				if (-1 != listenfd_)
				{
					close(listenfd_);
					listenfd_ = -1;
				}

				timer_ptr_->stop();

				for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
				{
					linker_handles_[i]->trd_->stop();
				}
			}


			return ok;
		};

		void stop() 
		{
			if (stop_)
			{
				return;
			}

			close(listenfd_);
			listenfd_ = -1;

			timer_ptr_->stop();

			event_ptr_->stop();

			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->trd_->async_task(std::bind(&LinkerHandle::close_all, linker_handles_[i].get()));
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(200));

			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				while (linker_handles_[i]->trd_->task_count() > 0)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
				}

				linker_handles_[i]->trd_->stop();
			}

			stop_ = true;
		};

		bool is_stop() 
		{
			return stop_;
		};

		//methods send plain data
		int send_by_fd(int fd, const char* data, unsigned int data_len) 
		{
			int ret = send(fd, data, data_len, 0);
			if (-1 == ret)
			{
				std::ostringstream oss;
				oss << "send to fd[" << fd << "] failed,err:" << strerror(errno);
				stdxLogWarn(oss.str());
			}
			return ret;
		};
        
		int send_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len)
		{
			return send_by_fd(linker->fd(), data, data_len);
		};

		int broadcast_data(const char* data, unsigned int data_len) 
		{
			std::string s(data, data_len);//make a copy

			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->trd_->async_task(std::bind(&LinkerHandle::broadcast, linker_handles_[i].get(), s, data_len));
			}

			return 0;
		};

		//make a Packet struct stream,then send the stream data
		int send_packet_data_by_fd(int fd, const char* data, unsigned int len, const std::shared_ptr<PacketHead>& head_ptr) 
		{
			auto buffer = serialize(data, len, head_ptr);
			return send_by_fd(fd, buffer.data(), buffer.size());
		};

		int send_packet_data_by_linker(const shared_ptr<TcpLinker>& linker, const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr) 
		{
			auto buffer = serialize(data, data_len, head_ptr);
			return send_by_linker(linker, buffer.data(), buffer.size());
		};

		int broadcast_packet_data(const char* data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr) 
		{
			auto buffer = serialize(data, data_len, head_ptr);
			return broadcast_data(buffer.data(), buffer.size());
		};

		//get a copy of linkers_ vectore
		vector<shared_ptr<TcpLinker>> get_linkers() 
		{
			std::vector<std::shared_ptr<TcpLinker>> v;
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				for (auto l : linker_handles_[i]->linkers_)
				{
					if (l.second)
					{
						auto ptr = std::make_shared<TcpLinker>(l.second->fd(), l.second->port(), l.second->ip());
						v.push_back(ptr);
					}
				}
			}

			return v;
		};
	public:
		//set callbacks
		void set_on_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn) 
		{
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->set_on_accept(fn);
			}
		};

		void set_on_close(const std::function<void(int fd, unsigned short port, string ip)>& fn) 
		{
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->set_on_close(fn);
			}
		};

		void set_on_data(const std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet> pkt_ptr)>& fn) 
		{
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->set_on_data(fn);
			}
		};

		//set connection timeout value,defualt is 30000 milliseconds
		inline void set_conn_timeout(unsigned int tm_milliseconds) { conn_time_out_ = tm_milliseconds; };

		//return connection status
		std::string status() 
		{
			std::ostringstream oss;
			oss << "Status:" << std::endl;
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				oss << "Connection count " << i << ":" << linker_handles_[i]->linkers_.size() << std::endl;
				oss << "Task count " << i << ":" << linker_handles_[i]->trd_->task_count() << std::endl;
			}

			return oss.str();
		};

	private:
		bool create_listen_fd() 
		{
			listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
			if (-1 == listenfd_)
			{
				std::ostringstream oss;
				oss << "create listen socekt fd failed, err:" << strerror(errno);
				stdxLogError(oss.str());
				return false;
			}

			int flag = fcntl(listenfd_, F_GETFL, 0);
			if (flag < 0)
			{
				std::ostringstream oss;
				oss << "get socket flag failed, err:" << strerror(errno);
				stdxLogError(oss.str());
				return false;
			}
			fcntl(listenfd_, F_SETFL, flag | O_NONBLOCK);

			struct sockaddr_in addr_in;
			memset(&addr_in, 0, sizeof(sockaddr_in));
			addr_in.sin_family = AF_INET;
			addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
			addr_in.sin_port = htons(port_);

			if (-1 == bind(listenfd_, (sockaddr*)&addr_in, sizeof(addr_in)))
			{
				std::ostringstream oss;
				oss << "bind socket failed, err:" << strerror(errno);
				stdxLogError(oss.str());

				return false;
	}

			if (-1 == listen(listenfd_, SOMAXCONN))
			{
				std::ostringstream oss;
				oss << "listen to socket failed, err:" << strerror(errno);
				stdxLogError(oss.str());
				return false;
			}

			return true;
		};

		void timer_check_fd(stdx::timer::Timer* /*timer*/, int /*id*/) 
		{
			for (unsigned int i(0); i < handle_cnt_ && i < linker_handles_.size(); ++i)
			{
				linker_handles_[i]->trd_->async_task(std::bind(&LinkerHandle::task_check_fd, linker_handles_[i].get(), conn_time_out_));
			}
		};

		//IO event callback       
		void on_event_read(int fd) 
		{
			if (fd == listenfd_) //accept
			{
				linker_handles_[fd%handle_cnt_]->trd_->async_task(std::bind(&LinkerHandle::task_accept, linker_handles_[fd%handle_cnt_].get(), fd));

			}
			else
			{
				linker_handles_[fd%handle_cnt_]->trd_->async_task(std::bind(&LinkerHandle::task_read, linker_handles_[fd%handle_cnt_].get(), fd));
			}
		};

		//dispatch linker to it's own thread
		void dispatch_accept(int fd, unsigned short port, std::string ip) 
		{
			linker_handles_[fd%handle_cnt_]->trd_->async_task(std::bind(&LinkerHandle::add_linker, linker_handles_[fd%handle_cnt_].get(), fd, port, ip));
		};

	private:
		//listen port
		unsigned short port_;
		int listenfd_;
		std::atomic<bool> stop_;

		//IO event
		std::shared_ptr<stdx::ioevent::IOEvent> event_ptr_;

		//timer
		std::shared_ptr<stdx::timer::Timer> timer_ptr_;
		unsigned int conn_time_out_;

		//group linkers by fd
		std::vector<std::shared_ptr<LinkerHandle>> linker_handles_;
		unsigned int handle_cnt_;
	};//end class Impl

#endif // WIN32

	TcpListener::TcpListener(unsigned int port, unsigned int read_thread_count)
	{
		impl_.reset(new Impl(port, read_thread_count));
	}

	TcpListener::~TcpListener()
	{
		impl_.reset();
	}

	bool TcpListener::start()
	{
		return impl_->start();
	}

	void TcpListener::stop()
	{
		return impl_->stop();
	}

	bool TcpListener::is_stop()
	{
		return impl_->is_stop();
	}

	int TcpListener::send_by_fd(int fd, const char * data, unsigned int data_len)
	{
		return impl_->send_by_fd(fd, data, data_len);
	}

	int TcpListener::send_by_linker(const shared_ptr<TcpLinker>& linker, const char * data, unsigned int data_len)
	{
		return impl_->send_by_linker(linker, data, data_len);
	}

	int TcpListener::broadcast_data(const char * data, unsigned int data_len)
	{
		return impl_->broadcast_data(data, data_len);
	}

	int TcpListener::send_packet_data_by_fd(int fd, const char * data, unsigned int len, const std::shared_ptr<PacketHead>& head_ptr)
	{
		return impl_->send_packet_data_by_fd(fd, data, len, head_ptr);
	}

	int TcpListener::send_packet_data_by_linker(const shared_ptr<TcpLinker>& linker, const char * data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr)
	{
		return impl_->send_packet_data_by_linker(linker, data, data_len, head_ptr);
	}

	int TcpListener::broadcast_packet_data(const char * data, unsigned int data_len, const std::shared_ptr<PacketHead>& head_ptr)
	{
		return impl_->broadcast_packet_data(data, data_len, head_ptr);
	}

	std::vector<shared_ptr<TcpLinker>> TcpListener::get_linkers()
	{
		return impl_->get_linkers();
	}

	void TcpListener::set_on_accept(const std::function<void(int fd, unsigned short port, string ip)>& fn)
	{
		impl_->set_on_accept(fn);
	}

	void TcpListener::set_on_close(const std::function<void(int fd, unsigned short port, string ip)>& fn)
	{
		impl_->set_on_close(fn);
	}

	void TcpListener::set_on_data(const std::function<void(int fd, unsigned short port, string ip, std::shared_ptr<Packet>pkt_ptr)>& fn)
	{
		impl_->set_on_data(fn);
	}

	void TcpListener::set_conn_timeout(unsigned int tm_milliseconds)
	{
		impl_->set_conn_timeout(tm_milliseconds);
	}

	std::string TcpListener::status()
	{
		return impl_->status();
	}
	

}}//end namespace