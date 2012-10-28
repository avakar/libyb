#ifndef LIBYB_UTILS_NONCOPYABLE_HPP
#define LIBYB_UTILS_NONCOPYABLE_HPP

namespace yb {

namespace noncopyable_ {

class noncopyable
{
protected:
	noncopyable() {}

private:
	noncopyable(noncopyable const &);
	noncopyable & operator=(noncopyable const &);
};

} // namespace noncopyable_

using noncopyable_::noncopyable;

} // namespace yb

#endif // LIBYB_UTILS_NONCOPYABLE_HPP
