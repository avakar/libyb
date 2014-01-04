#ifndef LIBYB_ASYNC_DETAIL_TASK_FWD_HPP
#define LIBYB_ASYNC_DETAIL_TASK_FWD_HPP

#include "../cancel_level.hpp"
#include "task_result.hpp"
#include "../../utils/noncopyable.hpp"
#include "../../utils/except.hpp"
#include "../cancellation_token.hpp"
#include <memory> // unique_ptr
#include <exception> // exception_ptr, exception

#include <type_traits> // conditional

namespace yb {

template <typename R>
class task;

namespace detail {

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

template <typename R>
struct task_value_traits
{
public:
	static size_t const storage_size = sizeof(R);
	static size_t const storage_alignment = std::alignment_of<R>::value;

	template <typename U>
	static task<R> from_value(U && u);

	R get_value();
	void move_from(task<R> & other);
	void clear_value();
};

template <typename R>
struct task_value_traits<R &>
{
public:
	static size_t const storage_size = sizeof(R *);
	static size_t const storage_alignment = std::alignment_of<R *>::value;

	static task<R &> from_value(R & u);

	R & get_value();
	void move_from(task<R &> & other);
	void clear_value();
};

template <>
struct task_value_traits<void>
{
	static size_t const storage_size = 0;
	static size_t const storage_alignment = 1;

	static task<void> from_value();

	void get_value();
	void move_from(task<void> &);
	void clear_value();
};

}

class task_wait_preparation_context;
class task_wait_finalization_context;

template <typename R>
class task_base;

template <typename R>
class task
	: private detail::task_value_traits<R>
{
public:
	typedef R result_type;

	task() throw();
	using detail::task_value_traits<R>::from_value;
	static task<R> from_exception(std::exception_ptr exc) throw();
	static task<R> from_task(task_base<result_type> * task_impl) throw();

	task(task && o);
	task & operator=(task && o) throw();
	~task();

	void clear() throw();

	bool empty() const throw();
	bool has_task() const throw();
	bool has_value() const throw();
	bool has_exception() const throw();

	R get();
	std::exception_ptr exception() const throw();
	void rethrow();

	void prepare_wait(task_wait_preparation_context & ctx);
	void finish_wait(task_wait_finalization_context & ctx);

	void cancel(cancel_level cl);

	// task shall not be null; returns the result after a potential synchronous wait
	task<result_type> cancel_and_wait();

	std::unique_ptr<task_base<result_type> > release();

	template <typename F>
	auto continue_with(F f) -> decltype(f(*(task<R>*)0));

	template <typename F>
	typename detail::task_then_type<R, F>::type then(F f);

	template <typename F>
	task<R> follow_with(F f);

	template <typename P>
	task<R> keep_alive(P && p);

	task<R> abort_on(cancel_level cl, cancel_level abort_cl = cl_abort);

	// task<void> only
	task<void> finish_on(cancel_level cl, cancel_level abort_cl = cl_abort);

	task<void> ignore_result();

	task<R> cancellable(cancellation_token & ct);
	task<R> finishable(cancellation_token & ct);

private:
	typedef task_base<R> * task_base_ptr;

	task_base_ptr & as_task() { return reinterpret_cast<task_base_ptr &>(m_storage); }
	task_base_ptr const & as_task() const { return reinterpret_cast<task_base_ptr const &>(m_storage); }

	enum kind_t { k_empty, k_value, k_exception, k_task };
	kind_t m_kind;
	typename std::aligned_storage<
		detail::yb_max<
			sizeof(task_base_ptr),
			sizeof(std::exception_ptr),
			detail::task_value_traits<R>::storage_size
			>::value,
		detail::yb_lcm<
			std::alignment_of<task_base_ptr>::value,
			std::alignment_of<std::exception_ptr>::value,
			detail::task_value_traits<R>::storage_alignment
		>::value
		>::type m_storage;


	friend detail::task_value_traits<R>;
	task(task const &) = delete;
	task & operator=(task const &) = delete;
};

task<void> operator|(task<void> && lhs, task<void> && rhs);
task<void> & operator|=(task<void> & lhs, task<void> && rhs);

template <typename F>
typename detail::task_protect_type<F>::type protect(F f);

template <typename F>
task<void> loop(F && f);

template <typename S, typename F>
task<void> loop(task<S> && t, F f);

namespace async {

template <typename R>
task<typename std::remove_const<typename std::remove_reference<R>::type>::type> value(R && v)
{
	typedef typename std::remove_const<typename std::remove_reference<R>::type>::type result_type;

	try
	{
		return task<result_type>::from_value(std::forward<R>(v));
	}
	catch (...)
	{
		return task<result_type>::from_exception(std::current_exception());
	}
}

task<void> value();

template <typename R>
task<R> raise(std::exception_ptr e)
{
	return task<R>::from_exception(std::move(e));
}

template <typename R, typename E>
task<R> raise(E && e)
{
	return task<R>::from_exception(yb::make_exception_ptr(std::forward<E>(e)));
}

template <typename R>
task<R> raise()
{
	return task<R>::from_exception(std::current_exception());
}

task<void> exit_guard(cancel_level cancel_threshold = cl_quit);

} // namespace async

struct nulltask_t
{
	template <typename T>
	operator task<T>() const;
};

static nulltask_t nulltask;

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_FWD_HPP
