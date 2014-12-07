#ifndef LIBYB_ASYNC_DETAIL_RUNNER_REGISTRY_HPP
#define LIBYB_ASYNC_DETAIL_RUNNER_REGISTRY_HPP

namespace yb {

class runner_registry;

struct runner_work
{
public:
	runner_work();

	void prepend_to_chain(runner_work *& first) throw();
	runner_work * work(runner_registry & rr) throw();

protected:
	virtual void do_work(runner_registry & rr) throw() = 0;

private:
	runner_work * m_next;
};

}

#endif // LIBYB_ASYNC_DETAIL_RUNNER_REGISTRY_HPP
