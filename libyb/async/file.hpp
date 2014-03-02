#ifndef LIBYB_ASYNC_FILE_HPP
#define LIBYB_ASYNC_FILE_HPP

#include "stream.hpp"
#include <stdint.h>

namespace yb {

class file
	: public stream
{
public:
	typedef uint64_t file_size_t;

	explicit file(yb::string_ref const & fname);
	~file();

	void clear();
	file_size_t size() const;
	task<buffer_view> read(buffer_policy policy, size_t max_size) override;
	task<size_t> write(buffer_ref buf) override;

private:
	struct impl;
	impl * m_pimpl;

	file(file const &);
	file & operator=(file const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_FILE_HPP
