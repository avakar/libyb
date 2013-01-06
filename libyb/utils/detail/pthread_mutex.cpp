#include "pthread_mutex.hpp"
#include <stdexcept>
using namespace yb;
using namespace yb::detail;

pthread_mutex::pthread_mutex()
{
	if (pthread_mutex_init(&m_mutex, 0) != 0)
		throw std::runtime_error("cannot create mutex");
}

pthread_mutex::~pthread_mutex()
{
	pthread_mutex_destroy(&m_mutex);
}

void pthread_mutex::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void pthread_mutex::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}
