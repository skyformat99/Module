﻿#ifndef __THREADPOOL_H_
#define __THREADPOOL_H_

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "locker.h"

template <typename T>
class threadpool
{
public:
	threadpool(int thread_num=8,int max_requests=10000);
	~threadpool();
	/*往请求队列中添加任务*/
	bool append(T *request);
private:
	/*工作线程运行函数，它不断的从工作队列中取出任务并执行之*/
	static void *worker(void *arg);
	void run();
private:
	int m_thread_number;
	int m_max_requests;
	pthread_t * m_threads;
	std::list<T*> m_workqueue;	/*任务请求请求队列*/ 
	locker m_queuelocker;	/*保护请求队列的互斥锁*/
	sem m_queuestat;		/*是否有任务需要处理*/
	bool m_stop;		/*是否结束线程*/
};

template <typename T>
threadpool<T>::threadpool(int thread_number,int max_requests) : 
	m_thread_number(thread_number),m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
	if((thread_number <= 0) || (max_requests <= 0))
	{
		throw std::exception();
	}
	m_threads = new pthread_t[m_thread_number];
	if(!m_threads)
	{
		throw std::exception();
	}

	/*创建thread_number个线程，并将他们都设置为脱离线程*/
	for(int i=0;i<thread_number;i++)
	{
		printf("create %dth thread\n",i);
		if(pthread_create(m_threads+i,NULL,worker,this)!=0)
		{
			delete[] m_threads;
			throw std::exception();
		}

		if(pthread_detach(m_threads[i]))
		{
			delete[] m_threads;
			throw std::exception();
		}
	}
}


template <typename T>
threadpool<T> :: ~threadpool()
{
	delete[] m_threads;
	m_stop=true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
	/**操作工作队列时一定要加锁*/
	m_queuelocker.lock();
	if(m_workqueue.size()>m_max_requests)
	{
		m_queuelocker.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();			//通知线程池，队列有任务到来
	return true;
}


template <typename T>
void * threadpool<T>::worker(void *arg)
{
	threadpool *pool =(threadpool *)arg;
	pool->run();
	return pool;
}


template <typename T>
void threadpool<T> ::run()
{
	while(!m_stop)
	{
		m_queuestat.wait();
		m_queuelocker.lock();
		if(m_workqueue.empty())
		{
			m_queuelocker.unlock();
			continue;
		}

		T *request=m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();
		if(!request)
		{
			continue;
		}
		request->process();/*运行请求的回调函数*/

	}
}

#endif