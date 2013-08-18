#include "win32_mutex.hpp"
using namespace yb::detail;

win32_mutex::win32_mutex()
{
	InitializeCriticalSection(&m_mutex);
}

win32_mutex::~win32_mutex()
{
	DeleteCriticalSection(&m_mutex);
}

void win32_mutex::lock()
{
	EnterCriticalSection(&m_mutex);
}

void win32_mutex::unlock()
{
	LeaveCriticalSection(&m_mutex);
}
