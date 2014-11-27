#include "../singleton.hpp"
#include <cassert>
#include <windows.h>
using namespace yb;

singleton_impl::singleton_impl()
	: m_lock(0), m_refcount(0)
{
}

singleton_impl::~singleton_impl()
{
	assert(m_refcount.load() == 0);

	size_t hEvent = m_lock.load() & ~(size_t)3;
	if (hEvent)
		::CloseHandle((HANDLE)hEvent);
}

void singleton_impl::addref()
{
	m_refcount.fetch_add(1);

	size_t lock = m_lock.load();

restart:
	if ((lock & 2) == 0)
	{
		if ((lock & 1) == 0)
		{
			if (!m_lock.compare_exchange_weak(lock, lock | 1))
				goto restart;
			lock |= 1;

			// XXX may throw
			this->do_construct();

			for (;;)
			{
				if (m_lock.compare_exchange_weak(lock, (lock | 2) & ~(size_t)1))
					break;
			}

			if (lock & ~(size_t)3)
				::SetEvent((HANDLE)(lock & ~(size_t)3));
		}
		else
		{
			size_t hEvent = (size_t)::CreateEvent(0, TRUE, FALSE, 0);
			if (hEvent == 0)
			{
				::Sleep(0);
				lock = m_lock.load();
				goto restart;
			}

			assert(hEvent % 4 == 0);

			if (!m_lock.compare_exchange_strong(lock, lock | hEvent))
			{
				::CloseHandle((HANDLE)hEvent);
				goto restart;
			}

			::WaitForSingleObject((HANDLE)hEvent, INFINITE);
		}
	}
}

void singleton_impl::release()
{
	if (m_refcount.fetch_sub(1) != 1)
		return;

	size_t lock = m_lock.load();

restart:
	if (lock & 1)
		return;

	if (!m_lock.compare_exchange_weak(lock, lock | 1))
		goto restart;
	lock |= 1;

	if (lock & 2)
	{
		if (m_refcount.load() == 0)
		{
			if (lock & ~(size_t)3)
				::ResetEvent((HANDLE)(lock & ~(size_t)3));
			this->do_destruct();

			for (;;)
			{
				if (m_lock.compare_exchange_weak(lock, lock & ~(size_t)2))
				{
					lock &= ~(size_t)2;
					break;
				}
			}
		}
	}

	for (;;)
	{
		if (m_lock.compare_exchange_weak(lock, lock & ~(size_t)1))
			break;
	}
}
