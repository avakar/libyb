#ifndef LIBYB_ASYNC_CANCEL_LEVEL_HPP
#define LIBYB_ASYNC_CANCEL_LEVEL_HPP

namespace yb {

enum cancel_level_constant_t
{
	cl_none,
	cl_quit,
	cl_abort,
	cl_kill
};

class cancel_level
{
public:
	cancel_level(int level = cl_none)
		: m_level(level)
	{
	}

	int get() const
	{
		return m_level;
	}

	friend bool operator==(cancel_level const & lhs, cancel_level const & rhs)
	{
		return lhs.m_level == rhs.m_level;
	}

	friend bool operator!=(cancel_level const & lhs, cancel_level const & rhs)
	{
		return !(lhs == rhs);
	}

	friend bool operator<(cancel_level const & lhs, cancel_level const & rhs)
	{
		return lhs.m_level < rhs.m_level;
	}

	friend bool operator>(cancel_level const & lhs, cancel_level const & rhs)
	{
		return rhs < lhs;
	}

	friend bool operator<=(cancel_level const & lhs, cancel_level const & rhs)
	{
		return !(rhs < lhs);
	}

	friend bool operator>=(cancel_level const & lhs, cancel_level const & rhs)
	{
		return !(lhs < rhs);
	}

private:
	int m_level;
};

} // namespace yb

#endif // LIBYB_ASYNC_CANCEL_LEVEL_HPP
