#include "thread_pool.h"

#include <cassert>
#include <map>
#include <mutex>
#include <queue>
#include <vector>
#include <string>

namespace itc
{

class thread_pool::impl
{
public:
	using job_id = thread_pool::job_id;

    impl(const std::vector<unsigned>& priorityWorkersCount)
	{
		for(size_t idx = 0; idx < priorityWorkersCount.size(); ++idx)
		{
			auto count = priorityWorkersCount[idx];
			jobs_sorted_.emplace_back();
			workers_.emplace_back();
            auto& workers_for_level = workers_.back();
			for(unsigned i = 0; i < count; i++)
			{
				std::string name = "pool_w:" + std::to_string(idx) + ":" + std::to_string(i);
				workers_for_level.emplace_back(make_thread(name));
			}
		}
	}

	~impl()
	{
		clear_jobs();
	}

	job_id add_job(task job, size_t priority)
	{
		std::lock_guard<std::mutex> l(guard_);

		auto& internalJob = add_job_internal(std::move(job), priority);
		return internalJob.id;
	}

	job_id change_priority(job_id id, size_t priority)
	{
		std::lock_guard<std::mutex> l(guard_);

		auto it = jobs_.find(id);
		if(it == jobs_.end())
		{
			return id;
		}

		job_info& job = it->second;
		if(job.is_working == true || job.priority == priority)
		{
			return id;
		}

		auto& new_job = add_job_internal(std::move(job.user_job), priority);
		new_job.waiters = std::move(job.waiters);
		return new_job.id;
	}

	future<void> get_future(job_id id)
	{
		std::lock_guard<std::mutex> l(guard_);
		//
		auto it = jobs_.find(id);
		if(it == jobs_.end())
		{
			return make_ready_future();
		}

		auto& job = it->second;
		job.waiters.emplace_back();
		return job.waiters.back().get_future();
	}

	void clear_jobs(const std::vector<thread_pool::job_id>& ids)
	{
		std::vector<thread_pool::job_id> working_jobs;
		working_jobs.reserve(8);
		{
			std::lock_guard<std::mutex> l(guard_);

			for(auto id : ids)
			{
				auto it = jobs_.find(id);
				if(it == jobs_.end())
				{
					continue;
				}

				auto& job = it->second;
				if(job.is_working == false)
				{
					job.user_job = []() {};
					call_waiters(job);
					continue;
				}
				working_jobs.emplace_back(id);
			}
		}
		for(auto id : working_jobs)
		{
			get_future(id).wait();
		}
	}

	void clear_jobs()
	{
		bool isEmpty = false;

		while(isEmpty == false)
		{
			{
				std::lock_guard<std::mutex> l(guard_);

				for(auto& p : jobs_)
				{
					auto& job = p.second;
					if(job.is_working == false)
					{
						job.user_job = []() {};
						call_waiters(job);
					}
				}
			}
			//
			wait_all();
			//
			{
				std::lock_guard<std::mutex> l(guard_);
				isEmpty = jobs_.empty();
			}
		}
	}

	void clear_job(job_id id)
	{
		{
			std::lock_guard<std::mutex> l(guard_);
			auto it = jobs_.find(id);
			if(it == jobs_.end())
			{
				return;
			}

			auto& job = it->second;
			if(job.is_working == false)
			{
				job.user_job = []() {};
				call_waiters(job);
				return;
			}
		}

		get_future(id).wait();
	}

	void wait_all()
	{
		auto activeJobs = _getActiveids();
		for(const auto& id : activeJobs)
		{
			get_future(id).wait();
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
private:
	struct job_info
	{
		job_id id = 0;
		size_t priority = 0;
		bool is_working = true;
		task user_job;
		std::vector<promise<void>> waiters;
	};

	struct job_handle
	{
		job_id id = 0;
		size_t priority = 0;
		inline bool operator<(const job_handle& i) const
		{
			return priority < i.priority;
		}
	};

	job_info& add_job_internal(task user_job, size_t priority)
	{
		if(jobs_sorted_.empty() == true)
		{
			// TODO assert
		}

		auto id = free_id_++;
        auto& job = jobs_[id];
		job.id = id;
		job.priority = priority;
		job.is_working = false;
		job.user_job = std::move(user_job);
		//
        job_handle handle;
		handle.id = job.id;
		handle.priority = job.priority;

        size_t queue_idx = std::min(jobs_sorted_.size() - 1, priority);
        jobs_sorted_[queue_idx].emplace(handle);
		add_invokes(priority);
		return job;
	}

	void add_invokes(size_t max_priority)
	{
		max_priority = std::min(workers_.size(), max_priority + 1);
		for(size_t pr = 0; pr < max_priority; pr++)
		{
			auto& workers = workers_[pr];
			for(auto& w : workers)
			{
				invoke(w.get_id(), [this, priority = pr]() { check_jobs(priority); });
			}
		}
	}
	//
	void check_jobs(size_t priority)
	{
		if(this_thread::notified_for_exit())
		{
			return;
		}

		task user_job;
		job_id id = 0;
		//
		{
			std::lock_guard<std::mutex> l(guard_);
			//
			size_t queue_idx = priority;
			for(size_t i = queue_idx; i < jobs_sorted_.size(); i++)
			{
				if(jobs_sorted_[i].empty() == false)
				{
					queue_idx = i;
                    break;
				}
			}
			auto& job_queue = jobs_sorted_[queue_idx];

			if(job_queue.empty() == true)
			{
				return;
			}
			const auto& handle = job_queue.top();

			id = handle.id;
			job_queue.pop();
			//
			auto& job = jobs_[id];
			if(job.user_job == nullptr)
			{
				cleanup_job(job);
				return;
			}
			if(job.is_working == true)
			{
				//assert(false);
				cleanup_job(job);
				return;
			}
			user_job = std::move(job.user_job);
			job.is_working = true;
		}
		////////////
		{
			user_job();
		}
		////////////
		{
			std::lock_guard<std::mutex> l(guard_);
			//
			auto& job = jobs_[id];
			cleanup_job(job);
		}
	}

	void cleanup_job(job_info& job)
	{
		call_waiters(job);
		jobs_.erase(job.id);
	}
	void call_waiters(job_info& job)
	{
		for(auto& waiter : job.waiters)
		{
			waiter.set_value();
		}
		job.waiters.clear();
	}

	std::vector<thread_pool::job_id> _getActiveids()
	{
		std::vector<thread_pool::job_id> jobs;
		std::lock_guard<std::mutex> l(guard_);
		//
		jobs.reserve(jobs_.size());
		for(const auto& job : jobs_)
		{
			jobs.push_back(job.first);
		}
		return jobs;
	}

	using workers_t = std::vector<itc::thread>;
	using priority_workers_t = std::vector<workers_t>;
	using jobs_queue = std::priority_queue<job_handle>;

	std::mutex guard_;
	job_id free_id_ = 1;
	priority_workers_t workers_;
	std::map<job_id, job_info> jobs_;
	std::vector<jobs_queue> jobs_sorted_;
};

////////////////////////////////////////////////////////////

thread_pool::thread_pool(const std::vector<unsigned>& workers_per_priority_level)
{
	impl_ = std::make_unique<impl>(workers_per_priority_level);
}

thread_pool::~thread_pool() = default;

thread_pool::job_id thread_pool::add_job(task job, size_t priority)
{
	return impl_->add_job(std::move(job), priority);
}

thread_pool::job_id thread_pool::change_priority(job_id id, size_t priority)
{
	return impl_->change_priority(id, priority);
}

future<void> thread_pool::get_future(job_id id)
{
	return impl_->get_future(id);
}

void thread_pool::clear()
{
	impl_->clear_jobs();
}

void thread_pool::stop(thread_pool::job_id id)
{
	impl_->clear_job(id);
}

void thread_pool::stop(const std::vector<thread_pool::job_id>& ids)
{
	impl_->clear_jobs(ids);
}

void thread_pool::wait_all()
{
	impl_->wait_all();
}
}
