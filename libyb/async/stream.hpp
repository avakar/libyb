#ifndef LIBYB_ASYNC_STREAM_HPP
#define LIBYB_ASYNC_STREAM_HPP

#include "../buffer.hpp"
#include "task.hpp"
#include "../vector_ref.hpp"
#include <stdint.h>
#include <functional>

namespace yb {

class stream
{
public:
	virtual task<buffer_view> read(buffer_policy policy, size_t max_size) = 0;
	virtual task<size_t> write(buffer_ref buf) = 0;
};

task<buffer_view> read(stream & s, buffer_policy policy, size_t max_size);
task<size_t> read(stream & s, uint8_t * ptr, size_t size);
task<void> read_all(stream & s, uint8_t * buffer, size_t size);
task<void> read_all(stream & s, std::function<bool(buffer_ref const & chunk)> const & handler, size_t max_chunk_size = 0);

task<size_t> write(stream & s, buffer_ref buf);
task<size_t> write(stream & s, uint8_t const * buffer, size_t size);
task<void> write_all(stream & s, buffer_ref buf);
task<void> write_all(stream & s, uint8_t const * buffer, size_t size);

task<void> copy(stream & sink, stream & source, size_t buffer_size = 256);
task<void> discard(stream & source, size_t buffer_size = 256);

} // namespace yb

#endif // LIBYB_ASYNC_STREAM_HPP
