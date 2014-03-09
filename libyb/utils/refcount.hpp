#ifndef LIBYB_UTILS_REFCOUNT_HPP
#define LIBYB_UTILS_REFCOUNT_HPP

#include <stdlib.h>

#ifndef _MSC_VER
#include <atomic>
#endif

namespace yb {

class refcount
{
public:
	explicit refcount(size_t initial);

	size_t addref();
	size_t release();

private:
#ifndef _MSC_VER
	std::atomic<size_t> m_value;
#else
	size_t m_value;
#endif
};

}

#endif // LIBYB_UTILS_REFCOUNT_HPP
