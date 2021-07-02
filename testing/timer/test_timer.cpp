#include "timer/timer.h"
#include "thread/task_thread_pool.h"

#include <iostream>

#include <chrono>

std::mutex mutx;
std::condition_variable cond;
void timer_fun(stdx::timer::Timer*t, int timer_id)
{
    std::cout << "timer :[" << timer_id << "] invoked" << std::endl;
    if(t && timer_id == 11)
    {        
        t->del_timer(timer_id);//only invoke once
    }
    if(t&&timer_id == 500)
    {
        t->del_timer(timer_id);
        t->add_timer(timer_id,10000,timer_fun);
    }    

    if(t&&timer_id == 20000)
    {
        t->del_all_timer();
        cond.notify_all();
    }
}

auto tp_add1 = std::chrono::system_clock::now();
int add1(0);

void timer_add1(stdx::timer::Timer*t, int timer_id)
{
    t->del_timer(timer_id);
    if(timer_id ==1)
    {
        tp_add1 = std::chrono::system_clock::now();
    }
    
    add1 += timer_id;
    
}

int test_add1()
{
    auto tp = std::make_shared<stdx::thread::TaskThreadPool>();
    if(!tp->start(6,10))
    {
        return -1;
    }

    stdx::timer::Timer t;
    if(!t.start(tp))
    {
        tp->stop();
        return -2;
    } 

    auto tp1 = std::chrono::system_clock::now();
    t.add_timer(1,1000,timer_add1);
    t.add_timer(2,1000,timer_add1);
    t.add_timer(3,1000,timer_add1);
    

    while(t.timer_count() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    t.stop();
    tp->stop();

    std::chrono::milliseconds d = std::chrono::duration_cast<std::chrono::milliseconds>(tp_add1 - tp1);
    std::cout << "test tiemer add1 the d is:" << d.count() << " the add1 is:" << add1 << std::endl;
    if(d.count() < 1000 || d.count() > 1500)
    {
        return -3;
    }

    if(add1 != 6)
    {        
        return -4;
    }

    return 0;
}

int exe_time(0);
void timer_del(stdx::timer::Timer*t, int timer_id)
{
    ++exe_time;
    if(2==exe_time)
    {
        t->del_timer(timer_id);
    }
}

int test_del()
{
    auto tp = std::make_shared<stdx::thread::TaskThreadPool>();
    if(!tp->start(2,4))
    {
        return -1;
    }

    stdx::timer::Timer t;
    if(!t.start(tp))
    {
        tp->stop();
        return -2;
    } 

    t.add_timer(1,1000,timer_del);

    while(t.timer_count() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    t.stop();
    tp->stop();

    if(exe_time != 2)
    {
        return -3;
    }

    return 0;
}


int main(int arc, char*argv[])
{
    if(arc == 1)//debug
    {
        //test_add1();
        //return 0;

        stdx::timer::Timer t;
        t.start();        
        
        t.add_timer(10,10000,timer_fun);
        t.add_timer(2,2000,timer_fun);
        t.add_timer(11,11000,timer_fun);
        t.add_timer(3,3000,timer_fun);
        t.add_timer(1,1000,timer_fun);
        t.add_timer(500,500,timer_fun);
        t.add_timer(20000,20000,timer_fun);

        {
            std::unique_lock<std::mutex> lck(mutx);
            cond.wait(lck);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        //t.stop();

        auto tp = std::make_shared<stdx::thread::TaskThreadPool>();
        tp->start(2,4);
        t.start(tp);     


        t.add_timer(10,10000,timer_fun);
        t.add_timer(2,2000,timer_fun);
        t.add_timer(11,11000,timer_fun);
        t.add_timer(3,3000,timer_fun);
        t.add_timer(1,1000,timer_fun);
        t.add_timer(500,500,timer_fun);
        t.add_timer(20000,20000,timer_fun);   
       
        {
            std::unique_lock<std::mutex> lck(mutx);
            cond.wait(lck);

        }
        
        std::this_thread::sleep_for(std::chrono::seconds(20));

        t.stop();
        tp->stop();
    }    
    else
    {
        std::string cmd(argv[1]);
        if("add1" == cmd)
        {
            return test_add1();
        }

        if("del" == cmd)
        {
            return test_del();
        }
    }
    

    return 0;
}