
#include "thread/task_thread_pool.h"

#include <thread>

#include <iostream>
#include <ctime>
#include <string>

std::condition_variable cond;
std::mutex mutx;


std::atomic<int> pushed_cnt(0);
std::atomic<int> recev_cnt(0);
void print (int& i,stdx::thread::TaskThreadPool* tp)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(i*10)); 

    std::cout << " task count:" 
        << tp->task_count() << " thread count:" << tp->thread_count() << " pushed/rcev:" << pushed_cnt <<"/"<<recev_cnt
        << " task is:"  << i << std::endl;
    
    ++recev_cnt;
       
    if(pushed_cnt == recev_cnt)
    {
        cond.notify_all();
    }
}

struct Person
{
    std::string name_;
    int age_;
    std::string dad(int dad_age)
    {
        std::string ret(name_);
        
        ret.append("'s age is:").append(std::to_string(age_)).append(". dad' age is:").append(std::to_string(dad_age));

        return ret;       
        
    };

    static std::string say(const std::string& name){return std::string("my name is ").append(name);};
};

int print_person(Person &person)
{
    std::cout << person.name_ << " is " << person.age_ <<" years old" << std::endl;
    
    ++recev_cnt;

    if(pushed_cnt == recev_cnt)
    {
        cond.notify_all();
    }

    return 0;
}


void debug_task_thrad_pool()
{

    //ConLogErr<<"TEST LOG ERROR" << std::endl;    
   // ConLogInfo << "test log info" << std::endl;
   // ConLogInfo << "hard ware threading support" << std::thread::hardware_concurrency()<<std::endl;

    stdx::thread::TaskThreadPool tp;
    tp.start();
    tp.set_task_timeout(100);

    std::thread th([&tp]
    {
        for(int i= 0; i < 100; ++i)
        {
            tp.async_task(print,i,&tp);
            //std::cout << "pushed " << i << ",task count:" << tp.task_count() << std::endl;
            ++pushed_cnt;
        }

        for(int i= 100; i <= 1000; i=i+100)
        {
            tp.async_task(print,i,&tp);
            std::cout << "pushed " << i << std::endl;
            ++pushed_cnt;
        }

        Person p = {"same", 23};
        
        tp.async_task(print_person,p);
        ++pushed_cnt;

    });
    th.join();

   
    std::unique_lock<std::mutex> lck(mutx);
    cond.wait(lck);
    

    tp.stop();

    
    

}

class Add{
    public:
        int add(int i, int j)
        {
            return i + j;
        }
};

int test_muli_task()
{
    stdx::thread::TaskThreadPool tp;
    tp.start(10,12);
    
    for(int i= 0; i < 1000; ++i)
    {
        tp.async_task(print,i,&tp);
        std::cout << "test_muli_task pushed " << i << std::endl;
        //std::this_thread::sleep_for(std::chrono::seconds(i));
    }

    int ret(0);
    if(tp.thread_count() <= 10)
    {
        ret = -1;
    }

    tp.stop();

    return ret;
}

int test_return()
{
    stdx::thread::TaskThreadPool tp;
    tp.start(2,2);

    auto fun_add = [](int i, int j)-> int{return i + j;};
    auto fun_times = [](int i, int j)-> int{return i*j;};

    auto add = tp.async_task(fun_add,2,3);
    if(add.get() != 5)
    {
        return -1;
    }

    auto times = tp.async_task(fun_times,3,4);
    if (times.get() != 12)
    {
        return -2;
    }

    tp.stop();

    return 0;    
}

int test_wait()
{
    stdx::thread::TaskThreadPool tp;
    tp.start(0,2);

    auto ret = tp.async_task([]{std::this_thread::sleep_for(std::chrono::seconds(5));});
    int wait_time(0);
    while (wait_time < 15)
    {
       if(std::future_status::timeout == ret.wait_for(std::chrono::milliseconds(500)))
       {
           ++wait_time;
           continue;
       }else
       {
           break;
       }
    }   

    tp.stop();

    std::cout << "wait time is" << wait_time << std::endl;

    if(wait_time > 8)
    {
        return 0;
    }else
    {
        return -1;
    }
    
}

int test_usage()
{
    stdx::thread::TaskThreadPool tp;
    tp.start(2,2);

    //function pointer
    tp.async_task(print,100,&tp);

    Person p = {"zhangsan",3};

    //static member function
    auto ret = tp.async_task(Person::say,"lisi");
    std::cout << "lisi " << ret.get() << std::endl;
    
    //normal membe function
    auto ret_dad = tp.async_task(std::bind(&Person::dad,p,33));
    std::cout << "dad " << ret_dad.get() << std::endl;

    //lanmbda
    auto tm = tp.async_task([](int i, int j)->int{return i*j;},4,5);

    std::cout <<" lambda 4 times 5 is:" << tm.get() << std::endl;

    tp.stop();

    return 0;
}


int main(int argc, char *argv[])
{
    if(argc == 1)//normal debug
    {        
        debug_task_thrad_pool();
        return 0;
    }

    std::string cmd(argv[1]);
    if("muti_task" == cmd)
    {
        test_muli_task();
        return 0;
    }

    if("async_return" == cmd)
    {
        return test_return();
    }

    if("wait" == cmd)
    {
        return test_wait();
    }

    if("usage" == cmd)
    {
        return test_usage();
    }
    return 1;
}
