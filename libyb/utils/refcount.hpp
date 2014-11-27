#ifndef LIBYB_UTILS_REFCOUNT_HPP
#define LIBYB_UTILS_REFCOUNT_HPP

#include <stdlib.h>
#include "atomic.hpp"

namespace yb {

class refcount
{
public:
	explicit refcount(size_t initial);

	size_t addref();
	size_t release();

private:
	yb::atomic_uint m_value;
};

}

#endif // LIBYB_UTILS_REFCOUNT_HPP
