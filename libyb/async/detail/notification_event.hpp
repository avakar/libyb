#ifndef LIBYB_ASYNC_DETAIL_NOTIFICATION_EVENT_HPP
#define LIBYB_ASYNC_DETAIL_NOTIFICATION_EVENT_HPP

#include "task_fwd.hpp"

namespace yb {

class notification_event
{
public:
	notification_event();
	~notification_event();
	notification_event(notification_event && o);
	notification_event & operator=(notification_event && o);

	void set();
	void reset();
	yb::task<void> wait();

private:
	union
	{
		void * m_win32_handle;
		int m_linux_eventfd;
	};

	notification_event(notification_event const &);
	notification_event & operator=(notification_event const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_NOTIFICATION_EVENT_HPP
