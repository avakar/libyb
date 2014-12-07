#ifndef LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP
#define LIBYB_ASYNC_DETAIL_TASK_IMPL_HPP

#include "sequential_composition_task.hpp"
#include "loop_task.hpp"
#include "cancel_level_upgrade_task.hpp"
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
task<R &> task_value_traits<R &>::from_value(R & u)
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
task<R> task_value_traits<R>::from_value(U && u)
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
		m_kind = o.m_kind;
		o.destroy<task_base_ptr>();
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
			task_base_ptr const & p = this->as_task();
			p.ptr->cancel(0, cl_kill);
			p.ptr->release();
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
bool task<R>::empty() const throw()
{
	return m_kind == k_empty;
}

template <typename R>
bool task<R>::has_value() const throw()
{
	return m_kind == k_value;
}

template <typename R>
bool task<R>::has_exception() const throw()
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
template <typename E>
bool task<R>::exception(E & e) const throw()
{
	try
	{
		if (this->has_exception())
		{
			std::exception_ptr eptr = this->exception();
			std::rethrow_exception(eptr);
		}
	}
	catch (E const & ee)
	{
		try
		{
			e = ee;
		}
		catch (...)
		{
			return false;
		}

		return true;
	}
	catch (...)
	{
		return false;
	}

	return false;
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
		m_kind = o.m_kind;
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
bool task<R>::has_task() const throw()
{
	return m_kind == k_task;
}

template <typename R>
bool task<R>::cancel(runner_registry * rr, cancel_level cl)
{
	while (m_kind == k_task)
	{
		task_base_ptr & p = this->as_task();
		if (p.cl >= cl)
			return false;

		task<R> t = p.ptr->cancel(rr, cl);
		if (t.empty())
		{
			p.cl = cl;
			return false;
		}

		*this = std::move(t);
	}

	return !this->has_task();
}

template <typename R>
bool task<R>::cancel(cancel_level cl)
{
	return this->cancel(0, cl);
}

template <typename R>
task<R> task<R>::cancel_and_wait()
{
	this->cancel(cl_kill);
	return std::move(*this);
}

template <typename R>
bool task<R>::replace(runner_registry & rr, task_completion_sink<R> & sink, task<R> && t)
{
	assert(this->has_task());

	cancel_level cl = this->as_task().cl;
	this->as_task().ptr->release();
	this->destroy<task_base_ptr>();
	m_kind = k_empty;

	*this = std::move(t);
	this->cancel(&rr, cl);
	return this->start(rr, sink);
}

template <typename R>
bool task<R>::start(runner_registry & rr, task_completion_sink<R> & sink)
{
	while (m_kind == k_task)
	{
		task_base_ptr const & p = this->as_task();
		task<R> t = p.ptr->start(rr, sink);
		if (t.empty())
			break;
		*this = std::move(t);
	}

	return !this->has_task();
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
task<R> then_impl(task<S> && t, F f, std::true_type, X)
{
	struct cont_t
	{
		cont_t(F && f)
			: f(std::move(f))
		{
		}

		cont_t(cont_t && o)
			: f(std::move(o.f))
		{
		}

		task<R> operator()(task<S> r)
		{
			if (r.has_exception())
				return async::raise<R>(r.exception());
			return f(r.get());
		}

		F f;
	};

	return t.continue_with(cont_t(std::move(f)));
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
task<R> then_impl(task<S> && t, F f, std::false_type, std::false_type)
{
	struct cont_t
	{
		cont_t(F && f)
			: f(std::move(f))
		{
		}

		cont_t(cont_t && o)
			: f(std::move(o.f))
		{
		}

		task<R> operator()(task<S> r)
		{
			if (r.has_exception())
				return async::raise<R>(r.exception());
			return async::value(f(r.get()));
		}

		F f;
	};

	return t.continue_with(cont_t(std::move(f)));
}

template <typename R, typename F, typename X>
task<R> then_impl(task<void> && t, F f, std::true_type, X)
{
	struct cont_t
	{
		cont_t(F && f)
			: f(std::move(f))
		{
		}

		cont_t(cont_t && o)
			: f(std::move(o.f))
		{
		}

		task<R> operator()(task<void> r)
		{
			if (r.has_exception())
				return async::raise<R>(r.exception());
			return f();
		}

		F f;
	};

	return t.continue_with(cont_t(std::move(f)));
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
	return detail::then_impl<unwrapped_type>(std::move(*this), std::forward<F>(f), detail::is_task<f_result_type>(), std::is_void<unwrapped_type>());
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
	return this->continue_with([p](task<R> && r) { return std::move(r); });
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
