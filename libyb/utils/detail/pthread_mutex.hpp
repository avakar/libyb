#ifndef LIBYB_UTILS_DETAIL_PTHREAD_MUTEX_HPP
#define LIBYB_UTILS_DETAIL_PTHREAD_MUTEX_HPP

#include <pthread.h>

namespace yb {
namespace detail {

class pthread_mutex
{
public:
	pthread_mutex();
	~pthread_mutex();

	void lock();
	void unlock();

private:
	pthread_mutex_t m_mutex;

	pthread_mutex(pthread_mutex const &);
	pthread_mutex & operator=(pthread_mutex const &);
};

class scoped_pthread_lock
{
public:
	explicit scoped_pthread_lock(pthread_mutex & m)
		: m(m)
	{
		m.lock();
	}

	~scoped_pthread_lock()
	{
		m.unlock();
	}

private:
	pthread_mutex & m;

	scoped_pthread_lock(scoped_pthread_lock const &);
	scoped_pthread_lock & operator=(scoped_pthread_lock const &);
};

}
}

#endif // LIBYB_UTILS_DETAIL_PTHREAD_MUTEX_HPP
