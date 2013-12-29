#ifndef LIBYB_ASYNC_STREAM_HPP
#define LIBYB_ASYNC_STREAM_HPP

#include "task.hpp"
#include "../vector_ref.hpp"
#include <stdint.h>
#include <functional>

namespace yb {

class stream
{
public:
	virtual task<size_t> read(uint8_t * buffer, size_t size) = 0;
	virtual task<size_t> write(uint8_t const * buffer, size_t size) = 0;

	task<void> read_all(uint8_t * buffer, size_t size);
	task<void> read_all(std::function<bool(buffer_ref const & chunk)> const & handler, size_t max_chunk_size = 0);
	task<void> write_all(uint8_t const * buffer, size_t size);

};

task<void> copy(stream & sink, stream & source, size_t buffer_size = 256);
task<void> discard(stream & source, size_t buffer_size = 256);

} // namespace yb

#endif // LIBYB_ASYNC_STREAM_HPP
