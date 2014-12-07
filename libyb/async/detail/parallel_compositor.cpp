#include "parallel_compositor.hpp"
using namespace yb;
using namespace yb::detail;

struct parallel_compositor::parallel_task
	: parallel_task_link
	, task_completion_sink<void>
{
	task<void> t;

	parallel_task_link * detach()
	{
		prev->next = next;
		next->prev = prev;
		return next;
	}

	void on_completion(runner_registry & rr, task<void> && r) override
	{
		if (!t.replace(rr, *this, std::move(r)))
			return;

		parallel_task_link * next = this->detach();
		delete this;

		// XXX is this not UB?
		if (next->next == next)
			static_cast<impl *>(next)->sink->on_completion(rr, async::value());
	}
};

void parallel_compositor::append_task(parallel_task_link * pt)
{
	pt->next = &m_tasks;
	pt->prev = m_tasks.prev;
	m_tasks.prev = pt;
	pt->prev->next = pt;
}

parallel_compositor::parallel_task_link * parallel_compositor::erase_task(parallel_task_link * pt)
{
	parallel_task_link * next = pt->next;

	pt->prev->next = pt->next;
	pt->next->prev = pt->prev;
	delete pt;

	return next;
}

parallel_compositor::parallel_compositor()
{
	m_tasks.next = &m_tasks;
	m_tasks.prev = &m_tasks;
	m_tasks.sink = nullptr;
}

parallel_compositor::~parallel_compositor()
{
	parallel_task_link * cur = m_tasks.next;
	while (cur != &m_tasks)
	{
		parallel_task * pt = static_cast<parallel_task *>(cur);
		cur = cur->next;

		delete pt;
	}
}

void parallel_compositor::add_task(task<void> && t)
{
	parallel_task * pt = new parallel_task();
	this->append_task(pt);
	pt->t = std::move(t);
}

void parallel_compositor::add_task(task<void> && t, task<void> && u)
{
	parallel_compositor c;

	parallel_task * pt1 = new parallel_task();
	c.append_task(pt1);
	parallel_task * pt2 = new parallel_task();
	c.append_task(pt2);

	pt1->t = std::move(t);
	pt2->t = std::move(u);

	this->append(std::move(c));
}

void parallel_compositor::append(parallel_compositor && o)
{
	o.m_tasks.prev->next = &m_tasks;
	o.m_tasks.next->prev = m_tasks.prev;

	m_tasks.prev->next = o.m_tasks.next;
	m_tasks.prev = o.m_tasks.prev;

	o.m_tasks.next = o.m_tasks.prev = &o.m_tasks;
}

bool parallel_compositor::empty() const
{
	return m_tasks.next == &m_tasks;
}

task<void> parallel_compositor::pop()
{
	assert(!this->empty());

	parallel_task * pt = static_cast<parallel_task *>(m_tasks.next);
	task<void> res = std::move(pt->t);
	this->erase_task(pt);
	return std::move(res);
}

task<void> parallel_compositor::start(runner_registry & rr, task_completion_sink<void> & sink)
{
	m_tasks.sink = &sink;

	parallel_task_link * cur = m_tasks.next;
	while (cur != &m_tasks)
	{
		parallel_task * pt = static_cast<parallel_task *>(cur);

		if (pt->t.start(rr, *pt))
			cur = this->erase_task(pt);
		else
			cur = cur->next;
	}

	return this->empty()? async::value(): yb::nulltask;
}

task<void> parallel_compositor::cancel(runner_registry * rr, cancel_level cl) throw()
{
	parallel_task_link * cur = m_tasks.next;
	while (cur != &m_tasks)
	{
		parallel_task * pt = static_cast<parallel_task *>(cur);

		if (pt->t.cancel(rr, cl))
			cur = this->erase_task(pt);
		else
			cur = cur->next;
	}

	return this->empty()? async::value(): yb::nulltask;
}
