#include "runner_registry.hpp"
using namespace yb;

runner_work::runner_work()
	: m_next(nullptr)
{
}

void runner_work::prepend_to_chain(runner_work *& first) throw()
{
	m_next = first;
	first = this;
}

runner_work * runner_work::work(runner_registry & rr) throw()
{
	runner_work * res = m_next;
	m_next = 0;
	this->do_work(rr);
	return res;
}
