#pragma once

#include "itc/future.hpp"
#include <memory>

namespace itc
{
//
class thread_pool
{
public:
	using job_id = uint64_t;

	thread_pool(const std::vector<unsigned>& workers_per_priority_level);
	thread_pool(thread_pool&&) = default;
	thread_pool& operator=(thread_pool&&) = default;
	thread_pool(const thread_pool&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;
	~thread_pool();

	job_id add_job(task job, size_t priority = 0);
	job_id change_priority(job_id id, size_t priority);
	future<void> get_future(job_id id);
	void clear();
	void stop(job_id id);
	void stop(const std::vector<job_id>& ids);
	void wait_all();

private:
    class impl;
	std::unique_ptr<impl> impl_;
};
}
