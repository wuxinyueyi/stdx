/* 
the io event facility, implemented with epoll on linux
 */

#ifndef __STDX_IOEVENT_IOEVENT_H_
#define __STDX_IOEVENT_IOEVENT_H_

#ifndef WIN32


#include <atomic>
#include <functional>
#include <thread>
#include <map>
#include <mutex>
#include <vector>

namespace stdx{namespace thread{
    class TaskThreadPool;
}}

namespace stdx{namespace ioevent{   

    class  IOEvent
    {
    private:
        struct EventCallbacks
        {
            std::function<void(int fd)> fn_read_;
            std::function<void(int fd)> fn_write_;

            EventCallbacks(const std::function<void(int fd)>& fn_event_read,const std::function<void(int fd)>& fn_event_write)
            :fn_read_(fn_event_read),fn_write_(fn_event_write)
            {
            };
        };
        
    private:
        int epollfd_;
        std::atomic<bool> stop_;
        std::atomic<bool> waiting_;

        //the global event handlers
        std::function<void(int fd)> callback_event_read_;
        std::function<void(int fd)> callback_event_write_;
        
        //thread that handle events,
        int thread_count_;
        std::thread event_thread_;

        //the task thead pool to hancle events
        std::shared_ptr<stdx::thread::TaskThreadPool> task_thread_pool_;

        //the evnet handlers for each fd.
        std::map<int,EventCallbacks> map_callback_;
        std::mutex mtx_;
    private:
        static int add_epoll_event(int epoll_fd,int fd,unsigned int events_mask);
        static int del_epoll_event(int epoll_fd,int fd,unsigned int events_mask);
        static int set_socket_noblock(int fd);
        //wait io events on fds added by add_fd() ina singel thread
        void wait_event();        
        void event_read(int fd);
        void event_write(int fd);

    public:
         IOEvent();
		 virtual ~ IOEvent();

        //start with two call back functions,fn_event_read is a callback when data comes,fn_event is a callback when fd writeable
        //thread_cnt means how many thead to handle io events,default is 1
        bool start(const std::function<void(int fd)>& fn_event_read,const std::function<void(int fd)>& fn_event_write,int thread_cnt=1);

        //add fd to event,wait_read means enable EPOLLIN flag, wait_write means enable EPOLLOUT flag
        int add_fd(int fd,bool wait_read = true,bool wait_write = false);

        //add fd to event and pecify a callback to thant fd.wait_read means enable EPOLLIN flag, wait_write means enable EPOLLOUT flag
        int add_fd(int fd,bool wait_read,bool wait_write,const std::function<void(int fd)>& fn_event_read,const std::function<void(int fd)>& fn_event_write);
        
        //remove fd from event
        int del_fd(int fd);
       
        //modify fd flags,wait_read means enable EPOLLIN flag, wait_write means enable EPOLLOUT flag
        int mod_fd(int fd,bool wait_read = true,bool wait_write = false);
        
        void stop();        
    };
        

}}

#endif // !WIN32

#endif //__STDX_IOEVENT_IOEVENT_H_
