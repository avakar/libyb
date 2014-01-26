#include "buffer.hpp"

yb::buffer::buffer()
	: m_deleter(0), m_ptr(0), m_size(0), m_capacity(0)
{
}

yb::buffer::buffer(buffer_deleter * deleter, uint8_t * ptr, size_t capacity)
	: m_deleter(deleter), m_ptr(ptr), m_size(capacity), m_capacity(capacity)
{
}

yb::buffer::buffer(buffer && o)
	: m_deleter(o.m_deleter), m_ptr(o.m_ptr), m_size(o.m_size), m_capacity(o.m_capacity)
{
	o.detach();
}

yb::buffer::~buffer()
{
	if (m_deleter)
		m_deleter->free(m_ptr, m_capacity);
}

yb::buffer & yb::buffer::operator=(buffer && o)
{
	swap(*this, o);
	return *this;
}

void yb::swap(buffer & lhs, buffer & rhs)
{
	using std::swap;
	swap(lhs.m_deleter, rhs.m_deleter);
	swap(lhs.m_ptr, rhs.m_ptr);
	swap(lhs.m_size, rhs.m_size);
	swap(lhs.m_capacity, rhs.m_capacity);
}

bool yb::buffer::empty() const
{
	return m_ptr == 0;
}

void yb::buffer::clear()
{
	if (m_ptr && m_deleter)
		m_deleter->free(m_ptr, m_capacity);
	this->detach();
}

void yb::buffer::detach()
{
	m_ptr = 0;
	m_size = 0;
	m_capacity = 0;
	m_deleter = 0;
}

uint8_t * yb::buffer::get() const
{
	return m_ptr;
}

size_t yb::buffer::size() const
{
	return m_size;
}

size_t yb::buffer::capacity() const
{
	return m_capacity;
}

yb::buffer_deleter * yb::buffer::deleter() const
{
	return m_deleter;
}

void yb::buffer::shrink(size_t size)
{
	if (size < m_size)
		m_size = size;
}

yb::buffer_ref yb::buffer::cref() const
{
	return buffer_ref(m_ptr, m_size);
}

yb::buffer::operator yb::buffer_ref() const
{
	return this->cref();
}

namespace {

struct policy_vtable
{
	void (*destroy)(void * ctx);
	void (*move)(void * ctx, void * target);
	yb::task<yb::buffer> (*fetch)(void * ctx, size_t min_capacity);
};

struct empty_policy
{
	policy_vtable const * vt;

	static void destroy(void * ctx)
	{
		(void)ctx;
	}

	static void move(void * ctx, void * target)
	{
		empty_policy * self = static_cast<empty_policy *>(ctx);
		*static_cast<empty_policy *>(target) = *self;
	}

	static yb::task<yb::buffer> fetch(void * ctx, size_t min_capacity)
	{
		(void)ctx;
		(void)min_capacity;
		return yb::async::raise<yb::buffer>(std::bad_alloc());
	}
};

struct static_policy
{
	policy_vtable const * vt;
	uint8_t * ptr;
	size_t capacity;
	yb::buffer_deleter * deleter;

	static void destroy(void * ctx)
	{
		static_policy * self = static_cast<static_policy *>(ctx);
		if (self->ptr && self->deleter)
			self->deleter->free(self->ptr, self->capacity);
	}

	static void move(void * ctx, void * target)
	{
		static_policy * self = static_cast<static_policy *>(ctx);
		*static_cast<static_policy *>(target) = *self;
		self->ptr = 0;
	}

	static yb::task<yb::buffer> fetch(void * ctx, size_t min_capacity)
	{
		static_policy * self = static_cast<static_policy *>(ctx);
		if (self->ptr == 0 || self->capacity < min_capacity)
			return yb::async::raise<yb::buffer>(std::bad_alloc());

		uint8_t * ptr = self->ptr;
		self->ptr = 0;
		return yb::async::value(yb::buffer(self->deleter, ptr, self->capacity));
	}
};

struct dynamic_deleter
	: public yb::buffer_deleter
{
	void free(uint8_t * ptr, size_t capacity) override
	{
		(void)capacity;
		delete[] ptr;
	}
};

dynamic_deleter g_dynamic_deleter;

struct dynamic_policy
{
	policy_vtable const * vt;
	size_t default_capacity;

	static void destroy(void * ctx)
	{
		(void)ctx;
	}

	static void move(void * ctx, void * target)
	{
		dynamic_policy * self = static_cast<dynamic_policy *>(ctx);
		*static_cast<dynamic_policy *>(target) = *self;
	}

	static yb::task<yb::buffer> fetch(void * ctx, size_t min_capacity)
	{
		dynamic_policy * self = static_cast<dynamic_policy *>(ctx);

		size_t capacity = (std::max)(self->default_capacity, min_capacity);
		uint8_t * ptr = new(std::nothrow) uint8_t[capacity];
		if (!ptr)
			return yb::async::raise<yb::buffer>(std::bad_alloc());
		return yb::async::value(yb::buffer(&g_dynamic_deleter, ptr, capacity));
	}
};

}

yb::buffer_policy::buffer_policy()
{
	static policy_vtable const vt = {
		&empty_policy::destroy,
		&empty_policy::move,
		&empty_policy::fetch,
	};

	empty_policy * policy = new(&m_storage) empty_policy();
	policy->vt = &vt;
}

yb::buffer_policy::buffer_policy(buffer_policy && o)
{
	policy_vtable const * ovt = reinterpret_cast<policy_vtable const *&>(o.m_storage);
	ovt->move(&o.m_storage, &m_storage);
}

yb::buffer_policy::~buffer_policy()
{
	policy_vtable const * vt = reinterpret_cast<policy_vtable const *&>(m_storage);
	vt->destroy(&m_storage);
}

yb::buffer_policy & yb::buffer_policy::operator=(buffer_policy && o)
{
	policy_vtable const * vt = reinterpret_cast<policy_vtable const *&>(m_storage);
	policy_vtable const * ovt = reinterpret_cast<policy_vtable const *&>(o.m_storage);

	vt->destroy(&m_storage);
	ovt->move(&o.m_storage, &m_storage);
	return *this;
}


yb::buffer_policy::buffer_policy(uint8_t * buffer, size_t size, buffer_deleter * deleter)
{
	static policy_vtable const vt = {
		&static_policy::destroy,
		&static_policy::move,
		&static_policy::fetch
	};

	static_policy * policy = new(&m_storage) static_policy();
	policy->vt = &vt;
	policy->ptr = buffer;
	policy->capacity = size;
	policy->deleter = deleter;
}

yb::buffer_policy::buffer_policy(buffer && buf)
{
	static policy_vtable const vt = {
		&static_policy::destroy,
		&static_policy::move,
		&static_policy::fetch
	};

	static_policy * policy = new(&m_storage) static_policy();
	policy->vt = &vt;
	policy->ptr = buf.get();
	policy->capacity = buf.capacity();
	policy->deleter = buf.deleter();
	buf.detach();
}

yb::task<yb::buffer> yb::buffer_policy::fetch(size_t min_capacity)
{
	policy_vtable const * vt = reinterpret_cast<policy_vtable const *&>(m_storage);
	return vt->fetch(&m_storage, min_capacity);
}

yb::buffer_policy yb::buffer_policy::dynamic(size_t default_capacity)
{
	static policy_vtable const vt = {
		&dynamic_policy::destroy,
		&dynamic_policy::move,
		&dynamic_policy::fetch
	};

	yb::buffer_policy res;
	dynamic_policy * policy = new(&res.m_storage) dynamic_policy();
	policy->vt = &vt;
	policy->default_capacity = default_capacity;
	return std::move(res);
}
