#ifndef LIBYB_UTILS_ATOMIC_HPP
#define LIBYB_UTILS_ATOMIC_HPP

#ifndef _MSC_VER
#include <atomic>
#endif

namespace yb {

class atomic_uint
{
public:
	typedef size_t value_type;

	atomic_uint(value_type initial = 0);

	value_type load();
	value_type fetch_add(value_type v);
	value_type fetch_sub(value_type v);
	bool compare_exchange_weak(value_type & expected, value_type desired);
	bool compare_exchange_strong(value_type & expected, value_type desired);

private:
#ifndef _MSC_VER
	std::atomic<value_type> m_value;
#else
	value_type m_value;
#endif
};

} // namespace yb

#endif // LIBYB_UTILS_ATOMIC_HPP
