#ifndef LIBYB_ASYNC_MOCK_STREAM_HPP
#define LIBYB_ASYNC_MOCK_STREAM_HPP

#include "stream.hpp"
#include "signal.hpp"
#include "../vector_ref.hpp"
#include <vector>

namespace yb {

class mock_stream
	: public yb::stream
{
public:
	mock_stream();
	void expect_read(yb::buffer_ref const & data, size_t max_chunk_size = 0);
	void expect_write(yb::buffer_ref const & data, size_t max_chunk_size = 0);

	task<size_t> read(uint8_t * buffer, size_t size);
	task<size_t> write(uint8_t const * buffer, size_t size);

public:
	struct action
	{
		enum { k_read, k_write } kind;
		std::vector<uint8_t> data;
		size_t max_chunk_size;
		yb::signal ready;
	};

	std::vector<action> m_expected_actions;
	size_t m_current_action;
	size_t m_action_pos;
};

} // namespace yb

#endif // LIBYB_ASYNC_MOCK_STREAM_HPP
