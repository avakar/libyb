#include "buffer.hpp"
#include "async/task.hpp"
#include "utils/refcount.hpp"
#include <cassert>
using namespace yb;

struct buffer::vtable_t
{
	void (*destroy)(uint8_t * ptr, size_t size);
	void (*share)(uint8_t *& ptr, size_t & size);
};

namespace {

struct buffer_policy_vtable
{
	task<buffer> (*fetch)(void * ctx, size_t max_size);
};

class buffer_policy_ref
{
public:
	explicit buffer_policy_ref(buffer_policy_vtable const ** origin);
	static task<buffer> fetch(void * ctx, size_t max_size);

private:
	buffer_policy_vtable const * m_vtable;
	buffer_policy_vtable const ** m_origin;
};

class buffer_policy_dynamic
{
public:
	explicit buffer_policy_dynamic(size_t size, bool shared);

	static task<buffer> fetch_unique(void * ctx, size_t max_size);
	static void destroy_unique(uint8_t * ptr, size_t size);

	static task<buffer> fetch_shared(void * ctx, size_t max_size);
	static void destroy_shared(uint8_t * ptr, size_t size);
	static void share_shared(uint8_t *& ptr, size_t & size);

private:
	buffer_policy_vtable const * m_vtable;
	size_t m_size;
};

class buffer_policy_static
{
public:
	explicit buffer_policy_static(buffer && buffer);
	static task<buffer> fetch(void * ctx, size_t max_size);

private:
	buffer_policy_vtable const * m_vtable;
	buffer m_buffer;
};

} // namespace

buffer_policy_ref::buffer_policy_ref(buffer_policy_vtable const ** origin)
{
	static buffer_policy_vtable const vtable = {
		/*fetch=*/&fetch,
	};

	if (*origin == &vtable)
	{
		// The origin is a ref too; reference its origin instead.
		buffer_policy_ref * original_ref = reinterpret_cast<buffer_policy_ref *>(origin);

		assert(original_ref->m_vtable != &vtable);
		m_vtable = original_ref->m_vtable;
		m_origin = original_ref->m_origin;
	}
	else
	{
		assert((*origin)->fetch);
		m_vtable = &vtable;
		m_origin = origin;
	}
}

task<buffer> buffer_policy_ref::fetch(void * ctx, size_t max_size)
{
	buffer_policy_ref * self = static_cast<buffer_policy_ref *>(ctx);

	assert((*self->m_origin)->fetch);
	return (*self->m_origin)->fetch(self->m_origin, max_size);
}

buffer_policy_dynamic::buffer_policy_dynamic(size_t size, bool shared)
	: m_size(size)
{
	static buffer_policy_vtable const vtable_unique = {
		/*fetch=*/&fetch_unique,
	};

	static buffer_policy_vtable const vtable_shared = {
		/*fetch=*/&fetch_shared,
	};

	m_vtable = shared? &vtable_shared: &vtable_unique;
}

task<buffer> buffer_policy_dynamic::fetch_unique(void * ctx, size_t max_size)
{
	buffer_policy_dynamic * self = static_cast<buffer_policy_dynamic *>(ctx);

	std::size_t actual_size = self->m_size;
	if (max_size && actual_size > max_size)
		actual_size = max_size;
	uint8_t * p = new(std::nothrow) uint8_t[actual_size];
	if (!p)
		return async::raise<buffer>(std::bad_alloc());

	static buffer::vtable_t const vtable = {
		/*destroy=*/&destroy_unique,
		/*share=*/nullptr,
	};

	return async::value(buffer(&vtable, p, actual_size));
}

void buffer_policy_dynamic::destroy_unique(uint8_t * ptr, size_t size)
{
	(void)size;
	delete[] ptr;
}

task<buffer> buffer_policy_dynamic::fetch_shared(void * ctx, size_t max_size)
{
	static size_t const universal_alignment = std::alignment_of<std::max_align_t>::value;
	size_t refcount_size = universal_alignment;
	while (refcount_size < sizeof(refcount))
		refcount_size += universal_alignment;

	buffer_policy_dynamic * self = static_cast<buffer_policy_dynamic *>(ctx);

	std::size_t actual_size = self->m_size;
	if (max_size && actual_size > max_size)
		actual_size = max_size;
	actual_size += refcount_size;

	uint8_t * p = new(std::nothrow) uint8_t[actual_size];
	if (!p)
		return async::raise<buffer>(std::bad_alloc());

	refcount * rc = new(p) refcount(1);
	p += refcount_size;

	static buffer::vtable_t const vtable = {
		/*destroy=*/&destroy_shared,
		/*share=*/&share_shared,
	};

	return async::value(buffer(&vtable, p, actual_size - refcount_size));
}

void buffer_policy_dynamic::destroy_shared(uint8_t * ptr, size_t size)
{
	(void)size;

	static size_t const universal_alignment = std::alignment_of<std::max_align_t>::value;
	size_t refcount_size = universal_alignment;
	while (refcount_size < sizeof(refcount))
		refcount_size += universal_alignment;

	ptr -= refcount_size;
	if (reinterpret_cast<refcount *>(ptr)->release() == 0)
		delete[] ptr;
}

void buffer_policy_dynamic::share_shared(uint8_t *& ptr, size_t & size)
{
	static size_t const universal_alignment = std::alignment_of<std::max_align_t>::value;
	size_t refcount_size = universal_alignment;
	while (refcount_size < sizeof(refcount))
		refcount_size += universal_alignment;

	uint8_t * p = ptr - refcount_size;
	reinterpret_cast<refcount *>(p)->addref();
}


buffer_policy_static::buffer_policy_static(buffer && buffer)
	: m_buffer(std::move(buffer))
{
	static buffer_policy_vtable const vtable = {
		/*fetch=*/&fetch,
	};

	m_vtable = &vtable;
}

task<buffer> buffer_policy_static::fetch(void * ctx, size_t max_size)
{
	buffer_policy_static * self = static_cast<buffer_policy_static *>(ctx);
	if (self->m_buffer.empty())
		return async::raise<buffer>(std::bad_alloc());

	return async::value(std::move(self->m_buffer));
}

void yb::swap(buffer & lhs, buffer & rhs)
{
	using std::swap;
	swap(lhs.m_vtable, rhs.m_vtable);
	swap(lhs.m_ptr, rhs.m_ptr);
	swap(lhs.m_size, rhs.m_size);
}

static buffer::vtable_t const g_empty_buffer_vtable = {
	/*destroy=*/nullptr,
	/*share=*/nullptr,
};

buffer::buffer()
	: m_vtable(&g_empty_buffer_vtable), m_ptr(nullptr), m_size(0)
{
}

buffer::buffer(uint8_t * ptr, size_t size)
	: m_vtable(&g_empty_buffer_vtable), m_ptr(ptr), m_size(size)
{
}

buffer::buffer(vtable_t const * vtable, uint8_t * ptr, size_t size)
	: m_vtable(vtable), m_ptr(ptr), m_size(size)
{
}

buffer::buffer(buffer && o)
	: m_vtable(o.m_vtable), m_ptr(o.m_ptr), m_size(o.m_size)
{
	o.m_vtable = &g_empty_buffer_vtable;
	o.m_ptr = nullptr;
	o.m_size = 0;
}

buffer::~buffer()
{
	this->clear();
}

buffer & buffer::operator=(buffer o)
{
	swap(*this, o);
	return *this;
}

void buffer::clear()
{
	if (m_vtable->destroy)
		m_vtable->destroy(m_ptr, m_size);

	m_vtable = &g_empty_buffer_vtable;
	m_ptr = nullptr;
	m_size = 0;
}

bool buffer::empty() const
{
	return m_size == 0;
}

uint8_t * buffer::data() const
{
	return m_ptr;
}

size_t buffer::size() const
{
	return m_size;
}

uint8_t * buffer::begin() const
{
	return m_ptr;
}

uint8_t * buffer::end() const
{
	return m_ptr + m_size;
}

bool buffer::sharable() const
{
	return m_vtable->share != nullptr;
}

buffer buffer::share() const
{
	assert(this->sharable());

	buffer res(m_vtable, m_ptr, m_size);
	m_vtable->share(res.m_ptr, res.m_size);
	return std::move(res);
}

void yb::swap(buffer_view & lhs, buffer_view & rhs)
{
	using std::swap;
	swap(lhs.m_buffer, rhs.m_buffer);
	swap(lhs.m_view, rhs.m_view);
}

buffer_view::buffer_view()
{
}

buffer_view::buffer_view(buffer_view && o)
	: m_buffer(std::move(o.m_buffer)), m_view(std::move(o.m_view))
{
}

buffer_view::buffer_view(buffer && buf)
	: m_buffer(std::move(buf)), m_view(m_buffer.data(), m_buffer.size())
{
}

buffer_view::buffer_view(buffer && buf, size_t length, size_t offset)
	: m_buffer(std::move(buf)), m_view(buffer_ref(m_buffer.data() + offset, m_buffer.data() + length))
{
}

buffer_view & buffer_view::operator = (buffer_view o)
{
	swap(*this, o);
	return *this;
}

void buffer_view::clear()
{
	m_buffer.clear();
	m_view.clear();
}

bool buffer_view::empty() const
{
	return m_view.empty();
}

bool buffer_view::clonable() const
{
	return m_buffer.sharable();
}

buffer_view buffer_view::clone() const
{
	assert(this->clonable());

	buffer_view res;
	res.m_buffer = m_buffer.share();
	res.m_view = m_view;
	return std::move(res);
}

buffer_ref const & buffer_view::operator*() const
{
	return m_view;
}

buffer_ref const * buffer_view::operator->() const
{
	return &m_view;
}

void buffer_view::pop_front(size_t size)
{
	m_view.pop_front(size);
}

void buffer_view::shrink(size_t offset, size_t size)
{
	if (offset > m_view.size())
		offset = m_view.size();
	if (size > m_view.size() - offset)
		size = m_view.size() - offset;

	m_view = buffer_ref(m_view.data() + offset, m_view.data() + offset + size);
}

buffer_policy::buffer_policy()
{
	static_assert(sizeof(buffer_policy_static) <= sizeof(m_storage), "XXX");
	new(&m_storage) buffer_policy_static(buffer());
}

buffer_policy::~buffer_policy()
{
}

buffer_policy::buffer_policy(size_t dynamic_buf_size, bool sharable)
{
	static_assert(sizeof(buffer_policy_dynamic) <= sizeof(m_storage), "XXX");
	new(&m_storage) buffer_policy_dynamic(dynamic_buf_size, sharable);
}

buffer_policy::buffer_policy(uint8_t * buf, size_t size)
{
	static_assert(sizeof(buffer_policy_static) <= sizeof(m_storage), "XXX");
	new(&m_storage) buffer_policy_static(buffer(buf, size));
}

buffer_policy::buffer_policy(buffer && buf)
{
	static_assert(sizeof(buffer_policy_static) <= sizeof(m_storage), "XXX");
	new(&m_storage) buffer_policy_static(std::move(buf));
}

buffer_policy::buffer_policy(buffer_policy && o)
{
	buffer_policy_vtable const * vtable = reinterpret_cast<buffer_policy_vtable const *&>(o.m_storage);
	memcpy(&m_storage, &o.m_storage, sizeof m_storage);
	new(&o.m_storage) buffer_policy_static(buffer());
}

buffer_policy buffer_policy::ref()
{
	buffer_policy_vtable const *& vtable = reinterpret_cast<buffer_policy_vtable const *&>(m_storage);

	buffer_policy res;
	new(&res.m_storage) buffer_policy_ref(&vtable);
	return std::move(res);
}

task<buffer> buffer_policy::fetch(size_t min_size, size_t max_size)
{
	buffer_policy_vtable const * vtable = reinterpret_cast<buffer_policy_vtable const *&>(m_storage);
	if (vtable->fetch)
		return vtable->fetch(&m_storage, max_size);
	return async::raise<buffer>(std::bad_alloc());
}

task<buffer_view> buffer_policy::copy(buffer_view && view)
{
	buffer_policy_vtable const * vtable = reinterpret_cast<buffer_policy_vtable const *&>(m_storage);

	struct cont_t
	{
		cont_t(buffer_view && view)
			: m_view(std::move(view))
		{
		}

		cont_t(cont_t && o)
			: m_view(std::move(o.m_view))
		{
		}

		buffer_view operator()(buffer && buf)
		{
			std::copy(m_view->begin(), m_view->end(), buf.data());
			return buffer_view(std::move(buf), m_view->size());
		}

		buffer_view m_view;
	};

	return vtable->fetch(&m_storage, view->size()).then(cont_t(std::move(view)));
}
