#include "tcp/tcp_listener.h"
#include "tcp/tcp_client.h"
#include "tcp/tcp_linker.h"
#include "tcp/packet.h"
#include "timer/timer.h"
#include "thread/task_thread_pool.h"
#include "stdx.h"
#include "log/log.h"


#include <iostream>
#include <string>

#include <mutex>
#include <condition_variable>
#include <future>

using namespace std;

mutex mtx;
condition_variable cond;
stdx::tcp::TcpListener tpl(1111);

void on_accept(int fd,unsigned short port,const string ip)
{
	LOG_INFO("on server accept [ip:%s:%d],fd:[%d]",ip.c_str(),port,fd);
}

void on_close(int fd,unsigned short /*port*/,const string /*ip*/)
{
	LOG_INFO("on server closed, fd[%d]", fd);
}

void on_data(int fd,unsigned short /*port*/,string ip, std::shared_ptr<stdx::tcp::Packet> pkt_ptr)
{
    std::string recv(pkt_ptr->data_.begin(),pkt_ptr->data_.end());

	LOG_INFO("on server recv from [%s], fd:[%d],data:%s", ip.c_str(), fd, recv.c_str());
    
    if(recv == "ping")
    {
        std::string rsp("pang");
		
		auto head = std::make_shared<stdx::tcp::PacketHead>(pkt_ptr->head_.type_|stdx::tcp::kPacketTypeMask,pkt_ptr->head_.id_);
		tpl.send_packet_data_by_fd(fd, rsp.data(), rsp.size(), head);
    }
}

void on_client_closed(int fd)
{
	LOG_INFO("on client closed fd:[%d]", fd);
}

void on_client_data(int fd, std::shared_ptr<stdx::tcp::Packet> pkt_ptr)
{
	LOG_INFO("on client data:%s from fd:[%d]", std::string(pkt_ptr->data_.data(),pkt_ptr->head_.length_).c_str(), fd);

}

void logstdx(int level,const std::string& log)
{
	switch (level)
	{
	case stdx::log::ELogLevel::info:
		LOG_INFO(log.c_str());
		break;

	case stdx::log::ELogLevel::debug:
		LOG_DEBUG(log.c_str());
		break;

	case stdx::log::ELogLevel::error:
		LOG_ERROR(log.c_str());
		break;

	case stdx::log::ELogLevel::warn:
		LOG_WARN(log.c_str());
		break;

	case stdx::log::ELogLevel::fatal:
		LOG_FATAL(log.c_str());
		break;
	default:
		break;
	}	
}

int main(int /*argc*/, char** /*argv*/)
{
	LOG_START("test.log", 9, 2);
	stdx::set_stdx_log_func(logstdx);	
    
    tpl.set_on_accept(on_accept);
    tpl.set_on_close(on_close);
    tpl.set_on_data(on_data);

    tpl.set_conn_timeout(100000);
    
    if(!tpl.start())
    {
		LOG_ERROR("start listener failed");
		LOG_STOP();
        return -1;
    }    

	
    stdx::tcp::TcpClient tc;
    tc.start();
    tc.set_on_closed(on_client_closed);
    tc.set_on_data(on_client_data);

    std::thread trd([&tc]{
        bool ret = tc.connect_to("127.0.0.1",1111);
        std::string ping("ping");
        
        if(!ret)
        {
            tc.stop();
            return;
        }
        
        int i(0);
        do{
            auto pkt = std::make_shared<stdx::tcp::PacketHead>(stdx::tcp::kDefaultHeartbeatReq,time(NULL));
           
            tc.send_packet_data(ping.data(),ping.size(),pkt);
            ++i;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            tc.disconnect();

			while (tc.is_connected())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}

            tc.connect_to("127.0.0.1",1111);

        }while(i < 20);
        tc.stop();    
       // cond.notify_all();
    });
    
   
    unique_lock<mutex> lck(mtx);
    cond.wait(lck);

    tpl.stop();
    tc.stop();
    trd.join();
	LOG_STOP();
    return 0;
}