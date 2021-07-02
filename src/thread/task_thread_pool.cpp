#include "thread/task_thread_pool.h"
#include "stdx.h"
#include <sstream>

namespace stdx{namespace thread{


    TaskThreadPoolImpl::TaskThreadPoolImpl():init_thread_count_(1),max_thread_count_(4),stop_(false),task_timeout_milli_(2000),valid_thead_count_(0)
    {            
    }

    TaskThreadPoolImpl::~TaskThreadPoolImpl()
    {
        //stop();
    }

    //start thread pool
    bool TaskThreadPoolImpl::start(unsigned int thread_count, unsigned int max_thread_count)
    {
        if(!threads_.empty())
        {            
            return false;//already started
        }

        //at least 1 thread       
        init_thread_count_ = thread_count;
        if(init_thread_count_ == 0)
        {
            init_thread_count_ = std::thread::hardware_concurrency();
            max_thread_count_ = 2*init_thread_count_;
        }else
        {
            max_thread_count_ = max_thread_count;
            if(max_thread_count_ < init_thread_count_)
            {
                max_thread_count_ = init_thread_count_;
            }
        }     

        std::lock_guard<std::mutex> lck(mutex_thread_);
        for (size_t i(0); i < init_thread_count_; ++i)
        {
            std::shared_ptr<std::thread> trd = std::make_shared<std::thread>(&TaskThreadPoolImpl::thread_fun,this);            
            threads_.emplace_back(std::make_shared<TaskThread>(true,trd));
            trd->detach();
            ++valid_thead_count_;
        }
        
        return true;
    }

    void TaskThreadPoolImpl::stop()
    {   
        if(stop_)         
        {
            return;
        }

        stop_ = true;
        cond_.notify_all();
     
        std::lock_guard<std::mutex> lck(mutex_thread_);
        threads_.clear();  
    }

    void TaskThreadPoolImpl::add_task(std::function<void()> fn)
    {   
        if(fn)
        {
            mutex_queue_.lock();
            queue_.emplace(std::make_shared<TaskElem>(fn,std::chrono::system_clock::now()));
            mutex_queue_.unlock();

            cond_.notify_one(); 
        }        
        
    }

    unsigned int TaskThreadPoolImpl::task_count()
    {
        return queue_.size();
    }
    
    unsigned int TaskThreadPoolImpl::thread_count()
    {
        return valid_thead_count_;
    }

    void TaskThreadPoolImpl::set_task_timeout(unsigned int t_milliseconds)
    {
        task_timeout_milli_ = t_milliseconds;
    }

    void TaskThreadPoolImpl::thread_fun()
    {
        std::shared_ptr<TaskElem> ta; 
        std::chrono::duration<int,std::milli> d_max_wait(0);

        while (!stop_)
        {   
            ta.reset();
            d_max_wait = std::chrono::milliseconds(0);
            //wait condition
            {
                std::unique_lock<std::mutex> lock(mutex_queue_);                             
                
                cond_.wait(lock,std::bind(&TaskThreadPoolImpl::no_blocking,this));                 

                if(!queue_.empty())
                {
                    if(queue_.front() && queue_.front()->fn_)
                    {
                        ta.swap(queue_.front());
                    }
                    
                    queue_.pop();     
                }   

                if(!queue_.empty())
                {                   
                   
                    if(queue_.back())
                    {
                        d_max_wait = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - queue_.back()->tp_);                    
                    }                    
                }            
            }

            
            if(!queue_.empty()
                && threads_.size() < max_thread_count_
                && (d_max_wait > std::chrono::milliseconds(task_timeout_milli_))
            )
            {
                std::shared_ptr<std::thread> trd = std::make_shared<std::thread>(&TaskThreadPoolImpl::thread_fun,this);   

                std::unique_lock<std::mutex> lck(mutex_thread_);
                threads_.emplace_back(std::make_shared<TaskThread>(false,trd));
                trd->detach();
                ++valid_thead_count_;
            }
          
            if(ta && ta->fn_)
            {                        
                ta->fn_();    
                if(task_timeout_milli_ > 0)
                {
                    std::chrono::milliseconds d = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ta->tp_);
                    if(d > std::chrono::milliseconds(task_timeout_milli_))  
                    {
                        std::ostringstream oss;
                        if(!name_.empty())
                        {
                            oss << "Task pool ["<< name_ <<"]:";
                        }
                        oss << "Task cost [" << d.count() << "] ms,pending task count: ["<< task_count() <<"]";
                        stdxLogWarn(oss.str());
                    }    
                }                    
            } 
            
            if(queue_.empty())
            {
                if(exit_idle_thread())       
                {                    
                    return;
                }
            }//end, if queue_ is empty   

        }//end while         
    
    }   

  
    bool TaskThreadPoolImpl::exit_idle_thread()
    {        
        //Reduce the number of thread to int_thread_count_*2                       
        if(valid_thead_count_ > (init_thread_count_*2))
        {    
            std::lock_guard<std::mutex> lck(mutex_thread_);             

            for (auto it = threads_.begin(); it != threads_.end() && (valid_thead_count_ > (init_thread_count_*2)); ++it)
            {
                if((*it)->id_ == std::this_thread::get_id())
                {
                    if((*it)->is_init_thread_)
                    {
                        return false;
                    }else
                    {
                        --valid_thead_count_;
                        threads_.erase(it);
                        return true;
                    }
                }
            }        
        }

        return false;
    }

     bool TaskThreadPoolImpl::no_blocking()
     {
         if(stop_)
         {
             return true;
         }

         if(!queue_.empty())
         {
             return true;
         }

         if(valid_thead_count_ < threads_.size())
         {
             return true;
         }

         return false;
     }

}}