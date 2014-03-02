#ifndef LIBYB_UTILS_REFCOUNT_HPP
#define LIBYB_UTILS_REFCOUNT_HPP

namespace yb {

class refcount
{
public:
	explicit refcount(size_t initial);

	size_t addref();
	size_t release();

private:
	size_t m_value;
};

}

#endif // LIBYB_UTILS_REFCOUNT_HPP
