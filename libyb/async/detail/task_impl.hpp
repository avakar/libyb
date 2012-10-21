#ifndef LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP

#include "value_task.hpp"
#include "sequential_composition_task.hpp"
#include "loop_task.hpp"
#include <type_traits>

namespace yb {

template <typename R>
task<R>::task()
	: m_kind(k_empty)
{
}

template <typename R>
task<R>::task(nulltask_t)
	: m_kind(k_empty)
{
}

template <typename R>
task<R>::task(task<R> && o)
	: m_kind(o.m_kind)
{
	switch (m_kind)
	{
	case k_result:
		new(&m_storage) task_result<R>(std::move(o.as_result()));
		break;
	case k_task:
		new(&m_storage) task_base_ptr(o.as_task());
		o.as_task().~task_base_ptr();
		o.m_kind = k_empty;
		break;
	}
}

template <typename R>
task<R>::task(task_base<result_type> * impl)
	: m_kind(k_task)
{
	new(&m_storage) task_base_ptr(impl);
}

template <typename R>
task<R>::task(std::unique_ptr<task_base<result_type> > impl)
	: m_kind(k_task)
{
	new(&m_storage) task_base_ptr(impl.release());
}

template <typename R>
task<R>::task(task_result<result_type> const & r)
	: m_kind(k_result)
{
	new(&m_storage) task_result<R>(r);
}

template <typename R>
task<R>::task(task_result<result_type> && r)
	: m_kind(k_result)
{
	new(&m_storage) task_result<R>(std::move(r));
}

template <typename R>
task<R>::task(std::exception_ptr exc)
	: m_kind(k_result)
{
	new(&m_storage) task_result<R>(std::move(exc));
}

template <typename R>
task<R>::~task()
{
	this->clear();
}

template <typename R>
void task<R>::clear() throw()
{
	switch (m_kind)
	{
	case k_task:
		{
			task_base_ptr p = this->as_task();
			p->cancel();
			p->wait();
			delete p;
		}
		this->as_task().~task_base_ptr();
		break;
	case k_result:
		this->as_result().~task_result();
		break;
	}

	m_kind = k_empty;
}

template <typename R>
void task<R>::normalize() throw()
{
	while (m_kind == k_task)
	{
		task<R> n = this->as_task()->run();
		if (n.empty())
			break;
		*this = n;
	}
}

template <typename R>
task<R>::operator void const *() const
{
	return m_kind != k_empty? this: nullptr;
}

template <typename R>
bool task<R>::operator!() const
{
	return m_kind == k_empty;
}

template <typename R>
bool task<R>::empty() const
{
	return m_kind == k_empty;
}

template <typename R>
task<R> & task<R>::operator=(task<R> && o)
{
	this->clear();
	kind_t new_kind = o.m_kind;

	switch (o.m_kind)
	{
	case k_task:
		new(&m_storage) task_base_ptr(o.as_task());
		o.as_task().~task_base_ptr();
		o.m_kind = k_empty;
		break;

	case k_result:
		new(&m_storage) task_result<R>(std::move(o.as_result()));
		break;
	}

	m_kind = new_kind;
	return *this;
}

template <typename R>
bool task<R>::has_task() const
{
	return m_kind == k_task;
}

template <typename R>
bool task<R>::has_result() const
{
	return m_kind == k_result;
}

template <typename R>
task_result<R> task<R>::get_result()
{
	assert(this->has_result());
	return std::move(this->as_result());
}

template <typename R>
void task<R>::prepare_wait(task_wait_preparation_context & ctx)
{
	assert(m_kind == k_task);
	task_base_ptr p = this->as_task();
	p->prepare_wait(ctx);
}

template <typename R>
void task<R>::finish_wait(task_wait_finalization_context & ctx)
{
	assert(m_kind == k_task);
	task_base_ptr p = this->as_task();
	task<R> n = p->finish_wait(ctx);

	if (!n.empty())
	{
		assert(m_kind == k_task);
		delete p;
		this->as_task().~task_base_ptr();
		m_kind = k_empty;

		*this = std::move(n);
	}
}

template <typename R>
void task<R>::cancel()
{
	if (m_kind == k_task)
		this->as_task()->cancel();
}

template <typename R>
task_result<R> task<R>::wait()
{
	assert(!this->empty());

	if (m_kind == k_task)
	{
		task_base_ptr p = this->as_task();
		task_result<R> r = p->wait();
		delete p;

		this->as_task().~task_base_ptr();
		m_kind = k_empty;
		return std::move(r);
	}

	assert(m_kind == k_result);
	return std::move(this->as_result());
}

template <typename R>
template <typename F>
auto task<R>::continue_with(F f) -> decltype(f(*(task_result<R>*)0))
{
	typedef decltype(f(*(task_result<R>*)0)) T;
	typedef typename T::result_type S;

	try
	{
		if (this->has_result())
			return f(std::move(this->as_result()));
		else
			return task<S>(new detail::sequential_composition_task<S, R, F>(std::move(*this), std::move(f)));
	}
	catch (...)
	{
		return task<S>(std::current_exception());
	}
}

namespace detail {

template <typename R, typename S, typename F, typename X>
task<R> then_impl(task<S> && t, F const & f, std::true_type, X)
{
	return t.continue_with([f](task_result<S> r) {
		return f(r.get());
	});
}

template <typename R, typename S, typename F>
task<R> then_impl(task<S> && t, F const & f, std::false_type, std::true_type)
{
	return t.continue_with([f](task_result<S> r) {
		f(r.get());
		return make_value_task();
	});
}

template <typename R, typename S, typename F>
task<R> then_impl(task<S> && t, F const & f, std::false_type, std::false_type)
{
	return t.continue_with([f](task_result<S> r) {
		return make_value_task(f(r.get()));
	});
}

template <typename R, typename F, typename X>
task<R> then_impl(task<void> && t, F const & f, std::true_type, X)
{
	return t.continue_with([f](task_result<void> r) -> task<R> {
		r.rethrow();
		return f();
	});
}

template <typename R, typename F>
task<R> then_impl(task<void> && t, F const & f, std::false_type, std::true_type)
{
	return t.continue_with([f](task_result<void> r) -> task<R> {
		r.rethrow();
		f();
		return make_value_task();
	});
}

template <typename R, typename F>
task<R> then_impl(task<void> && t, F const & f, std::false_type, std::false_type)
{
	return t.continue_with([f](task_result<void> r) -> task<R> {
		r.rethrow();
		return make_value_task(f());
	});
}

}

template <typename R>
template <typename F>
typename detail::task_then_type<R, F>::type task<R>::then(F f)
{
	typedef typename detail::task_then_type<R, F>::result_type f_result_type;
	typedef typename detail::task_then_type<R, F>::unwrapped_type unwrapped_type;
	return detail::then_impl<unwrapped_type>(std::move(*this), f, detail::is_task<f_result_type>(), std::is_void<unwrapped_type>());
}

inline task<void> make_value_task()
{
	return task<void>(task_result<void>());
}

template <typename R>
task<R> make_value_task(R const & v)
{
	try
	{
		return task<R>(v);
	}
	catch (...)
	{
		return task<R>(std::current_exception());
	}
}

template <typename R>
task<R> make_value_task(R && v)
{
	try
	{
		return task<R>(std::move(v));
	}
	catch (...)
	{
		return task<R>(std::current_exception());
	}
}

template <typename R>
task<R> make_value_task(std::exception_ptr e)
{
	return task<R>(e);
}

template <typename F>
typename detail::task_protect_type<F>::type protect(F f)
{
	try
	{
		return f();
	}
	catch (...)
	{
		return detail::task_protect_type<F>::type(std::current_exception());
	}
}

template <typename F>
task<void> loop(F f)
{
	return protect([&f] {
		return task<void>(new detail::loop_task<void, F>(make_value_task(), f));
	});
}

template <typename S, typename F>
task<void> loop(task<S> && t, F f)
{
	return protect([&t, &f] {
		return task<void>(new detail::loop_task<S, F>(std::move(t), f));
	});
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_HPP
