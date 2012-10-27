#ifndef LIBYB_ASYNC_DETAIL_TASK_FWD_HPP
#define LIBYB_ASYNC_DETAIL_TASK_FWD_HPP

#include "../task_result.hpp"
#include <memory> // unique_ptr
#include <exception> // exception_ptr, exception

#include <type_traits> // conditional

namespace yb {

template <typename R>
class task;

namespace detail {

template <typename T>
struct is_task
	: std::false_type
{
};

template <typename T>
struct is_task<task<T>>
	: std::true_type
{
};

template <typename T>
struct unwrap_task
{
	typedef T type;
};

template <typename T>
struct unwrap_task<task<T>>
{
	typedef T type;
};

template <typename R, typename F>
struct task_then_type
{
	typedef decltype((*(F*)0)(*(R*)0)) result_type;
	typedef typename unwrap_task<result_type>::type unwrapped_type;
	typedef task<unwrapped_type> type;
};

template <typename F>
struct task_then_type<void, F>
{
	typedef decltype((*(F*)0)()) result_type;
	typedef typename unwrap_task<result_type>::type unwrapped_type;
	typedef task<unwrapped_type> type;
};

template <typename F>
struct task_protect_type
{
	typedef decltype((*(F*)0)()) type;
};

}

struct task_cancelled
	: public std::exception
{
	task_cancelled()
		: std::exception("task cancelled")
	{
	}
};

class task_wait_preparation_context;
class task_wait_finalization_context;

template <typename R>
class task_base;

struct nulltask_t
{
};

static nulltask_t nulltask;

template <typename R>
class task
{
public:
	typedef R result_type;

	task();
	task(nulltask_t);
	task(task && o);
	explicit task(task_base<result_type> * impl);
	explicit task(std::unique_ptr<task_base<result_type> > impl);
	explicit task(task_result<result_type> const & value);
	explicit task(task_result<result_type> && value);
	task(std::exception_ptr exc);
	~task();

	task & operator=(task && o);

	void clear() throw();

	void normalize() throw();

	operator void const *() const;
	bool operator!() const;
	bool empty() const;
	bool has_task() const;
	bool has_result() const;
	task_result<result_type> get_result();

	void prepare_wait(task_wait_preparation_context & ctx);
	void finish_wait(task_wait_finalization_context & ctx);

	void cancel();

	// task shall not be null; returns the result after a potential synchronous wait
	task_result<result_type> wait();

	std::unique_ptr<task_base<result_type> > release();

	template <typename F>
	auto continue_with(F f) -> decltype(f(*(task_result<R>*)0));

	template <typename F>
	typename detail::task_then_type<R, F>::type then(F f);

	template <typename F>
	task<R> follow_with(F f);

private:
	typedef task_base<R> * task_base_ptr;

	enum kind_t { k_empty, k_result, k_task };

	task_base_ptr & as_task() { return reinterpret_cast<task_base_ptr &>(m_storage); }
	task_base_ptr const & as_task() const { return reinterpret_cast<task_base_ptr const &>(m_storage); }

	task_result<R> & as_result() { return reinterpret_cast<task_result<R> &>(m_storage); }
	task_result<R> const & as_result() const { return reinterpret_cast<task_result<R> &>(m_storage); }

	kind_t m_kind;
	typename std::aligned_storage<
		detail::yb_max<sizeof(task_base_ptr), sizeof(task_result<R>)>::value,
		detail::yb_lcm<
			std::alignment_of<task_base_ptr>::value,
			std::alignment_of<task_result<R>>::value
			>::value
		>::type m_storage;

	task(task const &);
	task & operator=(task const &);
};

task<void> make_value_task();

template <typename R>
task<R> make_value_task(R const & v);

template <typename R>
task<R> make_value_task(R && v);

template <typename R>
task<R> make_value_task(std::exception_ptr e);

task<void> operator|(task<void> && lhs, task<void> && rhs);
task<void> & operator|=(task<void> & lhs, task<void> && rhs);

template <typename F>
typename detail::task_protect_type<F>::type protect(F f);

template <typename F>
task<void> loop(F f);

template <typename S, typename F>
task<void> loop(task<S> && t, F f);

namespace async {

template <typename R>
task<R> value(R && v)
{
	return ::yb::make_value_task(std::forward<R>(v));
}

inline task<void> value()
{
	return ::yb::make_value_task();
}

template <typename R>
task<R> raise(std::exception_ptr e)
{
	return make_value_task<R>(std::move(e));
}

template <typename R, typename E>
task<R> raise(E && e)
{
	return make_value_task<R>(std::copy_exception(std::forward<E>(e)));
}

template <typename R>
task<R> raise()
{
	return make_value_task<R>(std::current_exception());
}

} // namespace async

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_FWD_HPP
