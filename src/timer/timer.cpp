#include "timer/timer.h"
#include "thread/task_thread_pool.h"
#include <thread>

namespace stdx{namespace timer{

    Timer::Timer():thread_pool_ptr_(NULL),stop_(false),running_(false)
    {

    }
    Timer::~Timer()
    {
        stop();
    }

    //bind to a task thread pool to exexuting the timer functions
    bool Timer::start(const std::shared_ptr<stdx::thread::TaskThreadPool>& thread_pool_ptr)
    {
        thread_pool_ptr_ = thread_pool_ptr;
        stop_ = false;
        
        return true;
    }
    
    //stop timer, if end_thread_pool is true, the task thrad pool binded will be stoped.
    void Timer::stop()
    {
        if(stop_)
        {
            return;
        }

        stop_ = true;
        
        cond_.notify_all();
        
        del_all_timer();

        if(timer_thread_.joinable())
        {
            timer_thread_.join();
        }
        
    }

    void Timer::add_timer(int id,long long duration_milliseconds_,const std::function<void(Timer* t,int timer_id)>& fn)
    {
        auto d = std::chrono::system_clock::now().time_since_epoch();
        TimerTask t;
        t.fun_=fn;
        t.id_=id;
        t.dur_ = std::chrono::milliseconds(duration_milliseconds_);
        t.run_point_ = std::chrono::system_clock::now() + t.dur_;

        std::lock_guard<std::mutex> lck(mutex_);
        tasks_.push(t);
        cond_.notify_all();

        if(!running_)
        {
            if(timer_thread_.joinable())
            {
                timer_thread_.join();
            }

            timer_thread_ = std::thread(std::bind(&Timer::timer_routine,this));
        }
    }

    void Timer::del_timer(int id)
    {
        std::priority_queue<TimerTask,std::vector<TimerTask>,Compare> new_task;

        std::lock_guard<std::mutex> lck(mutex_);
        while(!tasks_.empty())
        {
            if(tasks_.top().id_ != id)
            {
                new_task.push(tasks_.top());                
            }
            tasks_.pop();
        }
        tasks_ = new_task;
    }

    void Timer::del_all_timer()
    {
        std::lock_guard<std::mutex> lck(mutex_);
        while(!tasks_.empty())
        {
            tasks_.pop();
        }
    }

    void Timer::timer_routine()
    {    
        std::unique_lock<std::mutex> lck(cond_mutex_);
        while (!stop_)
        {            
            std::cv_status ret;
            
            if(!tasks_.empty())
            {
                running_ = true;
                ret = cond_.wait_until(lck,tasks_.top().run_point_);
            }else
            {
                break;
            }

            if(ret == std::cv_status::timeout)
            {
                while (!tasks_.empty() && !stop_)
                {
                    auto task = tasks_.top();
                    if(task.run_point_ <= std::chrono::system_clock::now())
                    {
                        TimerTask new_task(task);   
                        new_task.run_point_ += new_task.dur_;

                        mutex_.lock();    
                        tasks_.pop();                        
                        tasks_.push(new_task);
                        mutex_.unlock();                         
                        
                        if(thread_pool_ptr_)
                        {
                            thread_pool_ptr_->async_task([task,this]
                            {
                                task.fun_(this,task.id_);
                            });             
                        }
                        else
                        {
                            task.fun_(this,task.id_);
                        }                        
                    }else
                    {
                        break;
                    }
                }
                
            }           
        }   

        running_ = false;
    }
}}