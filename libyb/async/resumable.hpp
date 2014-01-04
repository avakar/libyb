#ifndef LIBYB_ASYNC_RESUMABLE_HPP
#define LIBYB_ASYNC_RESUMABLE_HPP

#include "task.hpp"
#include <functional>

namespace yb {

class resumer
{
public:
	struct impl;

	explicit resumer(impl * pimpl)
		: m_pimpl(pimpl)
	{
	}

	template <typename T>
	T operator%(yb::task<T> && t)
	{
		yb::task<T> res;
		this->run(t.continue_with([&res](yb::task<T> r) {
			res = yb::task<T>(std::move(r));
			return yb::async::value();
		}));
		return res.get();
	}

private:
	void run(yb::task<void> && t);

	impl * m_pimpl;

	resumer(resumer const &);
	resumer & operator=(resumer const &);
};

task<void> make_resumable(std::function<void(resumer &)> const & f);

template <typename T>
task<T> make_resumable(std::function<T(resumer &)> const & f)
{
	std::shared_ptr<yb::task<T>> res = std::make_shared<yb::task<T>>();
	return make_resumable([f, res](resumer & r) {
		*res = f(r);
		return async::value();
	}).then([res]() {
		return std::move(*res);
	});
}

template <typename F>
auto make_resumable(F f) -> task<decltype(f(*(resumer *)0))>
{
	typedef decltype(f(*(resumer *)0)) result_type;
	return make_resumable(std::function<result_type(resumer &)>(f));
}

template <typename F, typename P1, typename... P>
auto make_resumable(F && f, P1 && p1, P &&... p) -> task<decltype(f(std::forward<P1>(p1), std::forward<P>(p)..., *(resumer *)0))>
{
	return make_resumable(std::bind(std::forward<F>(f), std::forward<P1>(p1), std::forward<P>(p)..., std::placeholders::_1));
}

}

#define ybawait await %

#endif // LIBYB_ASYNC_RESUMABLE_HPP
