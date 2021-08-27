#include "dynamic_thread_pool.h"
#include "worker_thread.h"

dynamic_thread_pool::dynamic_thread_pool(int min_thread_num, int max_thread_num, int max_queue_size) : min_thread_num_(min_thread_num)
, max_thread_num_(max_thread_num)
, max_queue_size_(max_queue_size)
, is_runing_(true)
{

}

dynamic_thread_pool::~dynamic_thread_pool()
{
	stop();
}

void dynamic_thread_pool::start()
{
	check_min_threads();
}

void dynamic_thread_pool::stop()
{
	printf("stop threadpool\n");
	is_runing_ = false;
	std::unique_lock<std::mutex> guard(thread_lock_);
	for (thread_vec::iterator iter = thd_vec_.begin();
		iter != thd_vec_.end(); ++iter)
	{
		pworker_thread pw = *iter;
		if (pw)
		{
			pw->stop();
		}
	}
	for (thread_vec::iterator iter = thd_vec_.begin();
		iter != thd_vec_.end(); ++iter)
	{
		pworker_thread pw = *iter;
		if (pw)
		{
			delete pw;
		}
	}
}

bool dynamic_thread_pool::push(std::shared_ptr<task_callback> t)
{
	if (!is_runing_)
	{
		return false;
	}
	bool is_create_new = check_min_threads();
	if (!is_create_new)
	{
		shrink_threads();
	}
	return handle_task(t);
}

bool dynamic_thread_pool::handle_task(std::shared_ptr<task_callback> t)
{
	bool is_handled = false;
	std::unique_lock<std::mutex> guard(thread_lock_);
	for (thread_vec::iterator iter = thd_vec_.begin();
		iter != thd_vec_.end(); ++iter)
	{
		pworker_thread pw = *iter;
		if (NULL == pw)
		{
			continue;
		}
		if (pw->is_full())
		{
			continue;
		}
		is_handled = pw->push(t);
		break;
	}

	if (is_handled)
	{
		return true;
	}

	int thd_size = thd_vec_.size();
	if (thd_size < max_thread_num_)
	{
		int thd_id = thd_size + 1;
		pworker_thread pw = new worker_thread(thd_id, max_queue_size_);
		pw->start();
		thd_vec_.push_back(pw);
		pw->push(t);
		printf("create thread %d\n", thd_id);
		return true;
	}
	return false;
}

bool dynamic_thread_pool::check_min_threads()
{
	std::unique_lock<std::mutex> guard(thread_lock_);
	int thd_size = thd_vec_.size();
	if (thd_size < min_thread_num_)
	{
		for (int i = thd_size + 1; i <= min_thread_num_; ++i)
		{
			pworker_thread pw = new worker_thread(i, max_queue_size_);
			pw->start();
			thd_vec_.push_back(pw);
			printf("create thread %d\n", i);
		}
		return true;
	}
	return false;
}

bool dynamic_thread_pool::shrink_threads()
{
	std::unique_lock<std::mutex> guard(thread_lock_);
	int thd_size = thd_vec_.size();
	if (thd_size <= min_thread_num_)
	{
		return false;
	}
	int i = 0;
	for (thread_vec::iterator iter = thd_vec_.begin();
		iter != thd_vec_.end(); )
	{
		++i;
		if (i <= min_thread_num_)
		{
			++iter;
			continue;
		}
		pworker_thread pw = *iter;
		if (pw && pw->is_empty())
		{
			printf("shrink_threads %d\n", pw->get_thread_id());
			pw->stop();
			delete pw;
			iter = thd_vec_.erase(iter);
			continue;
		}
		++iter;
	}
	return true;
}
