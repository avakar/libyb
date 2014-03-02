#ifndef LIBYB_BUFFER_HPP
#define LIBYB_BUFFER_HPP

#include "vector_ref.hpp"
#include "async/detail/task_fwd.hpp"
#include <type_traits>

namespace yb {

class buffer
{
public:
	buffer();
	buffer(buffer && o);
	buffer(uint8_t * ptr, size_t size);
	~buffer();
	buffer & operator=(buffer o);

	struct vtable_t;
	buffer(vtable_t const * vtable, uint8_t * ptr, size_t size);

	void clear();
	bool empty() const;
	uint8_t * data() const;
	size_t size() const;
	uint8_t * begin() const;
	uint8_t * end() const;

	bool sharable() const;
	buffer share() const;

	friend void swap(buffer & lhs, buffer & rhs);

private:
	vtable_t const * m_vtable;
	uint8_t * m_ptr;
	size_t m_size;

	buffer(buffer const & o);
};

class buffer_view
{
public:
	buffer_view();
	buffer_view(buffer_view && o);
	explicit buffer_view(buffer && buf);
	buffer_view(buffer && buf, size_t length, size_t offset = 0);
	buffer_view & operator=(buffer_view o);

	void clear();
	bool empty() const;

	bool clonable() const;
	buffer_view clone() const;

	buffer_ref const & operator*() const;
	buffer_ref const * operator->() const;

	void pop_front(size_t size);
	void shrink(size_t offset, size_t size);

	friend void swap(buffer_view & lhs, buffer_view & rhs);

private:
	buffer m_buffer;
	buffer_ref m_view;

	buffer_view(buffer_view const &);
};

class buffer_policy
{
public:
	buffer_policy();
	~buffer_policy();
	buffer_policy(size_t dynamic_buf_size, bool sharable = false);
	buffer_policy(uint8_t * buf, size_t size);
	buffer_policy(buffer && buf);
	buffer_policy(buffer_policy && o);

	buffer_policy ref();
	task<buffer> fetch(size_t min_size = 1, size_t max_size = 0);
	task<buffer_view> copy(buffer_view && view);

private:
	std::aligned_storage<4*sizeof(void*)>::type m_storage;
};

} // namespace yb

#endif // LIBYB_BUFFER_HPP
