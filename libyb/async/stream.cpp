#include "stream.hpp"
using namespace yb;

task<void> stream::read_all(uint8_t * buffer, size_t size)
{
	return this->read(buffer, size).then([this, buffer, size](size_t transferred) -> task<void> {
		if (size == transferred)
			return this->read_all(buffer + transferred, size - transferred);
		else
			return make_value_task();
	});
}

task<void> stream::write_all(uint8_t const * buffer, size_t size)
{
	return this->write(buffer, size).then([this, buffer, size](size_t transferred) -> task<void> {
		if (size != transferred)
			return this->write_all(buffer + transferred, size - transferred);
		else
			return make_value_task();
	});
}
