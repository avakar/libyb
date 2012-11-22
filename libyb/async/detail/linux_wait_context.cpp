#include "linux_wait_context.hpp"
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
	// XXX
	m_pimpl->m_finished_tasks = 0;
}

void task_wait_preparation_context::add_poll_item(task_wait_poll_item const & item)
{
	// XXX
}

task_wait_preparation_context_impl * task_wait_preparation_context::get() const
{
	return m_pimpl.get();
}

void task_wait_preparation_context::set_finished()
{
	++m_pimpl->m_finished_tasks;
}

task_wait_checkpoint task_wait_preparation_context::checkpoint() const
{
	// XXX
	task_wait_checkpoint res;
	res.finished_task_count = m_pimpl->m_finished_tasks;
	return res;
}
