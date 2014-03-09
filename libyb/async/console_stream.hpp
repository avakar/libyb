#ifndef LIBYB_ASYNC_CONSOLE_STREAM_HPP
#define LIBYB_ASYNC_CONSOLE_STREAM_HPP

#include "stream.hpp"

namespace yb {

class console_stream
	: public stream
{
public:
	enum output_kind_t
	{
		out,
		err,
	};

	explicit console_stream(output_kind_t output_kind = out);
	console_stream(console_stream && o);
	~console_stream();
	console_stream & operator=(console_stream o);

	task<buffer_view> read(buffer_policy policy, size_t max_size) override;
	task<size_t> write(buffer_ref buf) override;

	friend void swap(console_stream & lhs, console_stream & rhs);

private:
	struct impl;
	impl * m_pimpl;

	console_stream(console_stream const &);
};

} // namespace yb

#endif // LIBYB_ASYNC_CONSOLE_STREAM_HPP
