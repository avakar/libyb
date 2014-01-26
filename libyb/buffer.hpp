#include "vector_ref.hpp"
#include "async/task.hpp"
#include <cassert>

namespace yb {

class buffer_deleter
{
public:
	virtual void free(uint8_t * ptr, size_t capacity) = 0;
};

class buffer
{
public:
	buffer();
	buffer(buffer_deleter * deleter, uint8_t * ptr, size_t capacity);
	buffer(buffer && o);
	~buffer();
	buffer & operator=(buffer && o);

	bool empty() const;
	void clear();
	void detach();

	friend void swap(buffer & lhs, buffer & rhs);

	uint8_t * get() const;
	size_t size() const;
	size_t capacity() const;
	buffer_deleter * deleter() const;
	buffer_ref cref() const;
	operator buffer_ref() const;

	void shrink(size_t size);

private:
	buffer_deleter * m_deleter;
	uint8_t * m_ptr;
	size_t m_size;
	size_t m_capacity;

	buffer(buffer const &);
	buffer & operator=(buffer const &);
};

class buffer_policy
{
public:
	buffer_policy();
	buffer_policy(buffer_policy && o);
	~buffer_policy();
	buffer_policy & operator=(buffer_policy && o);

	buffer_policy(uint8_t * buffer, size_t capacity, buffer_deleter * deleter = 0);
	buffer_policy(buffer && buf);
	static buffer_policy dynamic(size_t default_capacity);

	yb::task<buffer> fetch(size_t min_capacity = 0);

private:
	std::aligned_storage<sizeof(void *)* 4>::type m_storage;

	buffer_policy(buffer_policy const &);
	buffer_policy & operator=(buffer_policy const &);
};

} // namespace yb
