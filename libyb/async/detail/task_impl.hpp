#ifndef LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP

#include "sequential_composition_task.hpp"
#include "loop_task.hpp"
#include "cancel_level_upgrade_task.hpp"
#include "cancellation_token_task.hpp"
#include <type_traits>

namespace yb {

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


template <typename R>
static task<R &> task_value_traits<R &>::from_value(R & u)
{
	task<R &> res;
	reinterpret_cast<R *&>(res.m_storage) = &u;
	res.m_kind = task<R &>::k_value;
	return std::move(res);
}

template <typename R>
void task_value_traits<R &>::move_from(task<R &> & other)
{
	task<R &> * self = static_cast<task<R &> *>(this);
	reinterpret_cast<R *&>(self->m_storage) = reinterpret_cast<R *&>(other.m_storage);
}

template <typename R>
R & task_value_traits<R &>::get_value()
{
	task<R &> * self = static_cast<task<R &> *>(this);
	return *reinterpret_cast<R *&>(self->m_storage);
}

template <typename R>
void task_value_traits<R &>::clear_value()
{
}

template <typename R>
template <typename U>
static task<R> task_value_traits<R>::from_value(U && u)
{
	task<R> res;
	new(&res.m_storage) R(std::forward<U>(u));
	res.m_kind = task<R>::k_value;
	return std::move(res);
}

template <typename R>
R task_value_traits<R>::get_value()
{
	task<R> * self = static_cast<task<R> *>(this);
	return reinterpret_cast<R &&>(self->m_storage);
}

template <typename R>
void task_value_traits<R>::move_from(task<R> & other)
{
	task<R> * self = static_cast<task<R> *>(this);
	new(&self->m_storage) R(reinterpret_cast<R &&>(other.m_storage));
}

template <typename R>
void task_value_traits<R>::clear_value()
{
	task<R> * self = static_cast<task<R> *>(this);
	reinterpret_cast<R &>(self->m_storage).~R();
}

} // namespace detail

template <typename R>
task<R>::task(nulltask_t) throw()
	: m_kind(k_empty)
{
}

template <typename R>
task<R> task<R>::from_exception(std::exception_ptr exc) throw()
{
	task<R> res;
	new(&res.m_storage) std::exception_ptr(std::move(exc));
	res.m_kind = k_exception;
	return std::move(res);
}

template <typename R>
task<R> task<R>::from_task(task_base<result_type> * task_impl) throw()
{
	task<R> res;
	new(&res.m_storage) task_base<result_type>*(task_impl);
	res.m_kind = k_task;
	return std::move(res);
}

template <typename R>
task<R>::task(task<R> && o)
	: m_kind(k_empty)
{
	switch (o.m_kind)
	{
	case k_value:
		this->move_from(o);
		m_kind = k_value;
		break;
	case k_exception:
		new (&m_storage) std::exception_ptr(o.exception());
		m_kind = k_exception;
		break;
	case k_task:
		new(&m_storage) task_base_ptr(o.as_task());
		m_kind = k_task;
		this->destroy<task_base_ptr>();
		o.m_kind = k_empty;
		return;
	case k_empty:
		break;
	}

	o.clear();
}

template <typename R>
task<R>::~task()
{
	this->clear();
}

template <typename R>
template <typename T>
void task<R>::destroy() throw()
{
	reinterpret_cast<T &>(this->m_storage).~T();
}

template <typename R>
void task<R>::clear() throw()
{
	switch (m_kind)
	{
	case k_task:
		{
			task_base_ptr p = this->as_task();
			p->cancel_and_wait();
			delete p;
		}
		this->destroy<task_base_ptr>();
		break;
	case k_exception:
		this->destroy<std::exception_ptr>();
		break;
	case k_value:
		this->clear_value();
		break;
	case k_empty:
		break;
	}

	m_kind = k_empty;
}

template <typename R>
bool task<R>::empty() const
{
	return m_kind == k_empty;
}

template <typename R>
bool task<R>::has_value() const
{
	return m_kind == k_value;
}

template <typename R>
bool task<R>::has_exception() const
{
	return m_kind == k_exception;
}

template <typename R>
R task<R>::get()
{
	this->rethrow();

	assert(m_kind == k_value);
	return this->get_value();
}

template <typename R>
std::exception_ptr task<R>::exception() const throw()
{
	assert(m_kind == k_exception);
	return reinterpret_cast<std::exception_ptr const &>(this->m_storage);
}

template <typename R>
void task<R>::rethrow()
{
	assert(m_kind == k_value || m_kind == k_exception);
	if (m_kind == k_exception)
		std::rethrow_exception(reinterpret_cast<std::exception_ptr const &>(this->m_storage));
}

template <typename R>
task<R> & task<R>::operator=(task<R> && o) throw()
{
	this->clear();

	switch (o.m_kind)
	{
	case k_task:
		new(&m_storage) task_base_ptr(o.as_task());
		m_kind = k_task;
		o.destroy<task_base_ptr>();
		o.m_kind = k_empty;
		return *this;
	case k_value:
		this->move_from(o);
		break;
	case k_exception:
		new(&m_storage) std::exception_ptr(o.exception());
		break;
	case k_empty:
		break;
	}

	m_kind = o.m_kind;
	o.clear();
	return *this;
}

template <typename R>
bool task<R>::has_task() const
{
	return m_kind == k_task;
}

template <typename R>
void task<R>::prepare_wait(task_wait_preparation_context & ctx)
{
	if (m_kind == k_task)
	{
		task_base_ptr p = this->as_task();
		p->prepare_wait(ctx);
	}
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
		this->destroy<task_base_ptr>();
		m_kind = k_empty;

		*this = std::move(n);
	}
}

template <typename R>
void task<R>::cancel(cancel_level cl)
{
	if (m_kind == k_task)
	{
		task_base_ptr p = this->as_task();
		p->cancel(cl);
	}
}

template <typename R>
task<R> task<R>::cancel_and_wait()
{
	assert(!this->empty());

	if (m_kind == k_task)
	{
		task_base_ptr p = this->as_task();
		task<R> r = p->cancel_and_wait();
		delete p;

		this->destroy<task_base_ptr>();
		m_kind = k_empty;
		return std::move(r);
	}

	assert(m_kind == k_value || m_kind == k_exception);
	return std::move(*this);
}

template <typename R>
template <typename F>
auto task<R>::continue_with(F f) -> decltype(f(std::move(*(task<R>*)0)))
{
	typedef decltype(f(std::move(*(task<R>*)0))) T;
	typedef typename T::result_type S;

	try
	{
		if (this->has_value() || this->has_exception())
			return f(std::move(*this));
		else
			return task<S>::from_task(new detail::sequential_composition_task<S, R, F>(std::move(*this), std::move(f)));
	}
	catch (...)
	{
		return task<S>::from_exception(std::current_exception());
	}
}

namespace detail {

template <typename R, typename S, typename F, typename X>
task<R> then_impl(task<S> && t, F const & f, std::true_type, X)
{
	return t.continue_with([f](task<S> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		return f(r.get());
	});
}

template <typename R, typename S, typename F>
task<R> then_impl(task<S> && t, F const & f, std::false_type, std::true_type)
{
	return t.continue_with([f](task<S> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		f(r.get());
		return async::value();
	});
}

template <typename R, typename S, typename F>
task<R> then_impl(task<S> && t, F const & f, std::false_type, std::false_type)
{
	return t.continue_with([f](task<S> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		return async::value(f(r.get()));
	});
}

template <typename R, typename F, typename X>
task<R> then_impl(task<void> && t, F const & f, std::true_type, X)
{
	return t.continue_with([f](task<void> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		return f();
	});
}

template <typename R, typename F>
task<R> then_impl(task<void> && t, F const & f, std::false_type, std::true_type)
{
	return t.continue_with([f](task<void> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		f();
		return async::value();
	});
}

template <typename R, typename F>
task<R> then_impl(task<void> && t, F const & f, std::false_type, std::false_type)
{
	return t.continue_with([f](task<void> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		return async::value(f());
	});
}

template <typename R, typename F>
typename detail::task_then_type<R, F>::type then_impl2(task<R> && t, F const & f, std::false_type)
{
	typedef typename detail::task_then_type<R, F>::result_type f_result_type;
	typedef typename detail::task_then_type<R, F>::unwrapped_type unwrapped_type;
	return detail::then_impl<unwrapped_type>(std::move(t), f, detail::is_task<f_result_type>(), std::is_void<unwrapped_type>());
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

template <typename R>
template <typename F>
task<R> task<R>::follow_with(F f)
{
	return this->continue_with([f](task<R> r) -> task<R> {
		if (r.has_exception())
			return async::raise<R>(r.exception());
		R v(r.get());
		f(v);
		return async::value(std::move(v));
	});
}

template <typename R>
template <typename P>
task<R> task<R>::keep_alive(P && p)
{
	return this->follow_with([p](R const &) {});
}

template <>
template <typename F>
task<void> task<void>::follow_with(F f)
{
	return this->continue_with([f](task<void> r) -> task<void> {
		if (r.has_exception())
			return async::raise<void>(r.exception());
		f();
		return async::value();
	});
}

template <typename R>
task<R> task<R>::abort_on(cancel_level cl, cancel_level abort_cl)
{
	if (m_kind == k_task)
	{
		try
		{
			return task<R>::from_task(new detail::cancel_level_upgrade_task<R>(std::move(*this), cl, abort_cl));
		}
		catch (...)
		{
			return this->cancel_and_wait();
		}
	}

	return std::move(*this);
}

template <>
task<void> task<void>::finish_on(cancel_level cl, cancel_level abort_cl);

template <typename R>
task<void> task<R>::ignore_result()
{
	return this->then([](R const &) { return async::value(); });
}

template <>
task<void> task<void>::ignore_result();

template <typename R>
task<R> task<R>::cancellable(cancellation_token & ct)
{
	if (m_kind != k_task)
		return std::move(*this);

	detail::cancellation_token_core<R> * core = new detail::cancellation_token_core<R>();
	cancellation_token new_ct(core);
	task<R> core_task(new detail::cancellation_token_task<R>(core));

	core->m_task = std::move(*this);
	ct = new_ct;
	return std::move(core_task);
}

template <>
task<void> task<void>::finishable(cancellation_token & ct);

template <typename F>
typename detail::task_protect_type<F>::type protect(F f)
{
	try
	{
		return f();
	}
	catch (...)
	{
		return detail::task_protect_type<F>::type::from_exception(std::current_exception());
	}
}

} // namespace yb

#endif // LIBYB_ASYNC_DETAIL_TASK_HPP
