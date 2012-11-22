#include "unix_wait_context.hpp"
using namespace yb;

task_wait_preparation_context::task_wait_preparation_context()
	: m_pimpl(new task_wait_preparation_context_impl())
{
}

task_wait_preparation_context::~task_wait_preparation_context()
{
}

void task_wait_preparation_context::clear()
{
	m_pimpl->m_pollfds.clear();
	m_pimpl->m_finished_tasks = 0;
}

task_wait_checkpoint task_wait_preparation_context::checkpoint() const
{
	task_wait_checkpoint res;
	res.finished_task_count = m_pimpl->m_finished_tasks;
	res.poll_item_count = m_pimpl->m_pollfds.size();
	return res;
}

void task_wait_preparation_context::set_finished()
{
	++m_pimpl->m_finished_tasks;
}
