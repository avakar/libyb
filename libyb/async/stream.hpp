#ifndef LIBYB_ASYNC_STREAM_HPP
#define LIBYB_ASYNC_STREAM_HPP

#include "task.hpp"
#include <stdint.h>

namespace yb {

class stream
{
public:
	virtual task<size_t> read(uint8_t * buffer, size_t size) = 0;
	virtual task<size_t> write(uint8_t const * buffer, size_t size) = 0;

	task<void> read_all(uint8_t * buffer, size_t size);
	task<void> write_all(uint8_t const * buffer, size_t size);
};

} // namespace yb

#endif // LIBYB_ASYNC_STREAM_HPP
