#ifndef LIBYB_ASYNC_DETAIL_SEMAPHORE_HPP
#define LIBYB_ASYNC_DETAIL_SEMAPHORE_HPP

#include "task_fwd.hpp"

namespace yb {

class semaphore
{
public:
	semaphore();
	~semaphore();
	semaphore(semaphore && o);
	semaphore & operator=(semaphore && o);

	void set();
	yb::task<void> wait();

private:
	union
	{
		void * m_win32_handle;
		int m_linux_eventfd;
	};

	semaphore(semaphore const &);
	semaphore & operator=(semaphore const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_SEMAPHORE_HPP
