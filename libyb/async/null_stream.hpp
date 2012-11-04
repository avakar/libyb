#ifndef LIBYB_ASYNC_NULL_STREAM_HPP
#define LIBYB_ASYNC_NULL_STREAM_HPP

#include "stream.hpp"

namespace yb {

class null_stream
	: public stream
{
public:
	task<size_t> read(uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t const * buffer, size_t size);
};

} // namespace yb

#endif // LIBYB_ASYNC_NULL_STREAM_HPP
