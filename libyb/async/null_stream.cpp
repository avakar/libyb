#include "null_stream.hpp"
using namespace yb;

task<size_t> null_stream::read(uint8_t * buffer, size_t size)
{
	(void)buffer;
	(void)size;
	return async::value((size_t)0);
}

task<size_t> null_stream::write(uint8_t const * buffer, size_t size)
{
	(void)buffer;
	return async::value(size);
}
