#ifndef LIBYB_ASYNC_GROUP_HPP
#define LIBYB_ASYNC_GROUP_HPP

#include "task.hpp"

namespace yb {

class group
{
public:
	group();
	group(group && o);
	~group();
	group & operator=(group && o);
	void clear();

	static task<void> create(group & g);

	void post(task<void> && t);

private:
	struct impl;
	impl * m_pimpl;

	group(group const & o);
	group & operator=(group const & o);
};

} // namespace yb

#endif // LIBYB_ASYNC_GROUP_HPP
