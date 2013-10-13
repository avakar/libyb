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

typedef int cancel_level;

} // namespace yb

#endif // LIBYB_ASYNC_CANCEL_LEVEL_HPP
