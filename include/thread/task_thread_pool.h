/*
A task thread pool implementation base on std
*/
#ifndef __STDX_THREAD_TASKTHREADPOOL_H_
#define __STDX_THREAD_TASKTHREADPOOL_H_

#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <list>

namespace stdx {namespace thread{  
    class TaskThreadPoolImpl
    {
        public:
            TaskThreadPoolImpl();
            virtual ~TaskThreadPoolImpl();

            bool start(unsigned int thread_count, unsigned int max_thread_count);
            void stop();

            void add_task(std::function<void()> fn);

            unsigned int task_count();
            unsigned int thread_count();
            void set_task_timeout(unsigned int t_milliseconds);

            inline void set_name(const std::string& name){name_ = name;};
        private:
            void thread_fun();
            //to determin whether the currrent thread is init thread
            bool exit_idle_thread();
            bool no_blocking();
        private:
            struct TaskThread
            {
                std::shared_ptr<std::thread> trd_;
                
                //The thread created on thread pool initialization(in start() function). 
                //this thread will not be removed from the queue before stop() is called.             
                bool is_init_thread_;
                
                std::thread::id id_;//the thread id          
                TaskThread(bool is_init, std::shared_ptr<std::thread> trd):trd_(trd),is_init_thread_(is_init),id_(trd->get_id())
                {};
            };

            struct TaskElem
            {
                std::function<void()> fn_;
                std::chrono::system_clock::time_point tp_;

                TaskElem(std::function<void()> fn,const std::chrono::system_clock::time_point tp):fn_(fn),tp_(tp)
                {
                }
            };
            

        private:
            std::queue<std::shared_ptr<TaskElem>> queue_;
            std::list<std::shared_ptr<TaskThread>> threads_;
            std::mutex mutex_thread_;
            
            unsigned int init_thread_count_;
            unsigned int max_thread_count_;
            std::atomic<bool> stop_;
            

           // std::mutex mutex_wait_;        
            std::mutex mutex_queue_;
            std::condition_variable cond_;
            unsigned int task_timeout_milli_;//task timeout value

            std::atomic<unsigned int> valid_thead_count_;
            
            //the name specify this thread pool,default is empty
            std::string name_;
    };

    class TaskThreadPool
    { 
    public:
        TaskThreadPool():impl_ptr_(std::make_shared<TaskThreadPoolImpl>())
        {};

        virtual ~TaskThreadPool()
        {
            impl_ptr_->stop();
        };
                
        //start thread pool with init_thread_count threads。this pool can dynamicmally add threads up to max_thread_count
        //if init_thead_count is zero, the init thread count will be set to std::thread::hardware_concurrency()
        //init thread count will be set to 2, if std::thread::hardware_concurrency() is less than 2. 
        // if max_thread_count is less than 2*std::thread::hardware_concurrency() the max thread count will be set to 2*std::thread::hardware_concurrency()
        bool start(unsigned int init_thread_count=0, unsigned int max_thread_count=0)
        {
            return impl_ptr_->start(init_thread_count,max_thread_count);
        };

        //stop the thread pool
        void stop()
        {
            return impl_ptr_->stop();
        };

        //like std::async,run async tasks in thread pool.
        //more details see std::async
        template <class Fn, class... Args>
        auto async_task(Fn&& function, Args&& ...args)->std::future<decltype(function(args...))>
        {            
            //make a package_task pointer
            using FnType = decltype(function(args...));
            
            auto pack_task_ptr = std::make_shared<std::packaged_task<FnType()>>(std::bind(std::forward<Fn>(function), std::forward<Args>(args)...));
          
            auto task_future = pack_task_ptr->get_future(); 

            auto task_fn = [pack_task_ptr]
            {
                (*pack_task_ptr)();
            };

            impl_ptr_->add_task(task_fn);
            
            return task_future;       
        }

        //return the number of task in task queue
        unsigned int task_count()
        {
            return impl_ptr_->task_count();
        };

        //return the valid thread counts in the thread pool
        unsigned int thread_count()
        {
            return impl_ptr_->thread_count();
        };

        //set timeout value,if task timout,a timeout error log will be printed
        void set_task_timeout(unsigned int t_milliseconds)
        {
            impl_ptr_->set_task_timeout(t_milliseconds);
        }

        //set pool name,default is empty
        void set_name(const std::string& name)
        {
            impl_ptr_->set_name(name);
        }

    private:
        std::shared_ptr<TaskThreadPoolImpl> impl_ptr_;       
        
        
    };
    
}}

#endif