#ifndef LIBYB_ASYNC_SERIAL_PORT_HPP
#define LIBYB_ASYNC_SERIAL_PORT_HPP

#include "stream.hpp"
#include "../vector_ref.hpp"
#include <memory>

namespace yb {

class serial_port
	: public stream
{
public:
	serial_port();
	~serial_port();

	task<void> open(yb::string_ref const & name);
	void close();

	task<size_t> read(uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t const * buffer, size_t size);

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;

	serial_port(serial_port const &);
	serial_port & operator=(serial_port const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_SERIAL_PORT_HPP
