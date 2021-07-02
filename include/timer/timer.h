/*
a std chrono based timer
*/
#ifndef __STDX_TIMER_TIMER_H_
#define __STDX_TIMER_TIMER_H_

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

namespace stdx{namespace thread{
    class TaskThreadPool;
}}

namespace stdx{namespace timer{

class Timer
{
    struct TimerTask
    {
        TimerTask(){};
        TimerTask(const TimerTask& t)
        {
            id_ = t.id_;
            fun_ = t.fun_;
            run_point_ = t.run_point_;
            dur_ = t.dur_;
        };
        int id_;//the unique id
        std::chrono::system_clock::time_point run_point_;//the milliseconds since 1970.1.1
        std::function<void(Timer* t,int timer_id)> fun_;
        std::chrono::milliseconds dur_;
    };

    struct Compare
    {
        bool operator()(const TimerTask&l, const TimerTask& r)const
        {
            return r.run_point_ < l.run_point_;
        }
    };

    public:
        Timer();
        virtual ~Timer();

        //bind to a task thread pool to exexuting the timer functions
        //if thread_pool_ptr is null, will create a TaskThreadPool with 1 thread.
        bool start(const std::shared_ptr<stdx::thread::TaskThreadPool>& thread_pool_ptr=nullptr);
        
        //stop timer
        //the task thrad pool binded will not be stoped. 
        //the task thread pool that created by the timer will be stoped.
        void stop();
        
        void add_timer(int id,long long duration_milliseconds_,const std::function<void(Timer* t,int timer_id)>& fn);
        void del_timer(int id);
        void del_all_timer();
        inline int timer_count(){return tasks_.size();};

    private:
        void timer_routine();

    private:
        std::shared_ptr<stdx::thread::TaskThreadPool> thread_pool_ptr_;
        std::priority_queue<TimerTask,std::vector<TimerTask>,Compare> tasks_;
        std::mutex mutex_;        
        
        std::atomic<bool> stop_;
        std::condition_variable cond_;
        std::mutex cond_mutex_;

        std::atomic<bool> running_;

        std::thread timer_thread_;
        
};

}}

#endif