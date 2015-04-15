#ifndef LIBYB_ASYNC_STREAM_HPP
#define LIBYB_ASYNC_STREAM_HPP

#include "../buffer.hpp"
#include "task.hpp"
#include "../vector_ref.hpp"
#include <stdint.h>
#include <functional>

namespace yb {

struct istream
{
	virtual task<buffer_view> read(buffer_policy policy, size_t max_size) = 0;
};

class stream
	: public istream
{
public:
	virtual task<size_t> write(buffer_ref buf) = 0;
};

task<buffer_view> read(istream & s, buffer_policy policy, size_t max_size);
task<size_t> read(istream & s, uint8_t * ptr, size_t size);
task<void> read_all(istream & s, uint8_t * buffer, size_t size);
task<void> read_all(istream & s, std::function<bool(buffer_ref const & chunk)> const & handler, size_t max_chunk_size = 0);

task<size_t> write(stream & s, buffer_ref buf);
task<size_t> write(stream & s, uint8_t const * buffer, size_t size);
task<void> write_all(stream & s, buffer_ref buf);
task<void> write_all(stream & s, uint8_t const * buffer, size_t size);

task<void> copy(stream & sink, istream & source, size_t buffer_size = 256);
task<void> discard(istream & source, size_t buffer_size = 256);

} // namespace yb

#endif // LIBYB_ASYNC_STREAM_HPP
