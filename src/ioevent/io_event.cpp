#ifndef WIN32


#include "ioevent/io_event.h"
#include "stdx.h"
#include "thread/task_thread_pool.h"

#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <sstream>
#include <string.h>
#include <thread>
#include <unistd.h>

using namespace std;

namespace stdx{ namespace ioevent{
    #define LogErr std::cerr

    IOEvent:: IOEvent():epollfd_(-1),stop_(false),waiting_(false),thread_count_(1)
    {
    }

    IOEvent::~ IOEvent()
    {
    }

    int IOEvent::add_epoll_event(int epoll_fd,int fd,unsigned int events_mask)
    {
        struct epoll_event ev;
        ev.events = events_mask;
        ev.data.fd = fd;

        int ret = epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);        
        if (-1 == ret)
        {
            std::ostringstream oss;
            oss << "add epoll event failed, err" << strerror(errno);
            stdxLogWarn(oss.str());
        }

        return ret;
    }

    int IOEvent::del_epoll_event(int epoll_fd,int fd,unsigned int events_mask)
    {
        struct epoll_event ev;
        ev.events = events_mask;
        ev.data.fd = fd;

        int ret = epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ev);        
        if (-1 == ret)
        {
            std::ostringstream oss;
            oss << "del epoll event failed, err" << strerror(errno);
            stdxLogWarn(oss.str());
        }

        return ret;
    }

    int IOEvent::set_socket_noblock(int fd)
    {
        int flag = fcntl(fd,F_GETFL,0);
        if(flag < 0)
        {
            return -1;
        }

        if(fcntl(fd,F_SETFL,flag|O_NONBLOCK) < 0)
        {
            return -1;
        }

        return 0;
    } 

    bool IOEvent::start(const std::function<void(int fd)>& fn_event_read,const std::function<void(int fd)>& fn_event_write,int thread_cnt)
    {
        callback_event_read_ = fn_event_read;
        callback_event_write_ = fn_event_write;

        if(-1 != epollfd_)
        {
            return true;//already started
        }        

        bool ok(false);

        do
        {
            epollfd_= epoll_create1(0);
            if(-1 == epollfd_)
            {
                std::ostringstream oss;
                oss << "create epoll failed. err:"<< strerror(errno);
                stdxLogError(oss.str());
                break;
            }

            thread_count_ = thread_cnt;
            if(thread_count_ > 1)
            {
                task_thread_pool_ = std::make_shared<stdx::thread::TaskThreadPool>();
                task_thread_pool_->set_name("IOEvent thread pool");
                if(!task_thread_pool_->start(thread_count_,2*thread_count_))
                {                    
                    stdxLogError("Create task pool failed");                    
                    break;
                }
            }

            ok = true;
        }while(false);

        if(!ok)
        {
            if(-1 != epollfd_)
            {
                close(epollfd_);
                epollfd_=-1;
            }

            if(task_thread_pool_)
            {
                task_thread_pool_->stop();
                task_thread_pool_.reset();
            }

            return false;
        }
        
        event_thread_ = std::thread(std::bind(&IOEvent::wait_event,this));
              

        return true;
    }

    int IOEvent::add_fd(int fd,bool wait_read,bool wait_write)
    {
        set_socket_noblock(fd);
        unsigned int flag(EPOLLET);
        if(wait_read)
        {
            flag |= EPOLLIN;
        }

        if(wait_write)
        {
            flag |= EPOLLOUT;
        }
        return add_epoll_event(epollfd_,fd,flag);
    }

    int IOEvent::add_fd(int fd,bool wait_read,bool wait_write,const std::function<void(int fd)>& fn_event_read,const std::function<void(int fd)>& fn_event_write)
    {
        set_socket_noblock(fd);
        unsigned int flag(EPOLLET);
        if(wait_read)
        {
            flag |= EPOLLIN;
        }

        if(wait_write)
        {
            flag |= EPOLLOUT;
        }
        int ret = add_epoll_event(epollfd_,fd,flag);
        if(0==ret)
        {
            std::lock_guard<std::mutex> lck(mtx_);
            map_callback_.emplace(fd,EventCallbacks(fn_event_read,fn_event_write));
        }     

        return ret;   
    }

    int IOEvent::del_fd(int fd)//del df from event
    {
        if(-1 == epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,NULL))
        {
            std::ostringstream oss;
            oss << "del epoll fd[" << fd << "] failed, err:"  << strerror(errno);
            stdxLogWarn(oss.str());
        }

        if(map_callback_.find(fd) != map_callback_.end())
        {
            std::lock_guard<std::mutex> lck(mtx_);
            map_callback_.erase(fd);
        }

        return 0;
    }

    int IOEvent::mod_fd(int fd,bool wait_read,bool wait_write)
    {
        unsigned int flag(EPOLLET);
        if(wait_read)
        {
            flag |= EPOLLIN;
        }

        if(wait_write)
        {
            flag |= EPOLLOUT;
        }

        struct epoll_event ev;
        ev.events = flag;
        ev.data.fd = fd;

        int ret = epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&ev);

        if (-1 == ret)
        {
            std::ostringstream oss;
            oss << "add epoll event failed, err" << strerror(errno);
            stdxLogWarn(oss.str());            
        }

        return ret;
    }
    
    void IOEvent::stop()
    {
        if(stop_)
        {
            return;
        }

        stop_ = true;        

        close(epollfd_);
        map_callback_.clear();
       
        if(event_thread_.joinable())
        {
            event_thread_.join();
        }

        if(task_thread_pool_)
        {
            task_thread_pool_->stop();
        }
       
    }

    void IOEvent::wait_event()
    {   
        #define MAX_EVNTS_WAIT 64

        struct epoll_event events[MAX_EVNTS_WAIT];        

        int wait_milli(1000);

        while(!stop_)
        {
            int cnt = epoll_wait(epollfd_,events,MAX_EVNTS_WAIT,wait_milli);
            if(-1 == cnt)
            {
                int err(errno);
                if(err == EBADF
                || err == EINVAL)
                {
                    std::ostringstream oss;
                    oss << "epoll wait failed, err:" << strerror(err);
                    stdxLogError(oss.str());
                    
                    break;
                }                                
                

                continue;
            }

            if(0 == cnt) //timeout
            {
                continue;
            }

            for (int i(0); i < cnt; ++i)
            {
                if(events[i].data.fd == -1)
                {
                    continue;
                }
               
                if(events[i].events & EPOLLIN
                || events[i].events & EPOLLPRI)
                {
                    if(thread_count_ > 1 && task_thread_pool_)
                    {
                        int fd(events[i].data.fd);
                        task_thread_pool_->async_task(std::bind(&IOEvent::event_read,this,fd));
                    }
                    else
                    {
                        event_read(events[i].data.fd);
                    }                   
                }

                if(events[i].events & EPOLLOUT)
                {
                    if(thread_count_ > 1 && task_thread_pool_)
                    {
                        int fd(events[i].data.fd);
                        task_thread_pool_->async_task(std::bind(&IOEvent::event_write,this,fd));
                    }
                    else
                    {
                        event_write(events[i].data.fd);
                    }                   
                }                
            }
            
        }//end while   
    }  

    void IOEvent::event_read(int fd)
    {
        auto it = map_callback_.find(fd);
        if(it != map_callback_.end())
        {
            if(it->second.fn_read_)
            {
                it->second.fn_read_(fd);
            }                        
        }
        else if(callback_event_read_)
        {
            callback_event_read_(fd);
        }
    }
    
    void IOEvent::event_write(int fd)
    {
        auto it = map_callback_.find(fd);
        if(it != map_callback_.end())
        {
            if(it->second.fn_write_)
            {
                it->second.fn_write_(fd);
            }                        
        }
        else if(callback_event_write_)
        {
            callback_event_write_(fd);
        }
    }
}}

#endif // !WIN32


