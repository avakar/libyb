#include "mock_stream.hpp"
using namespace yb;

mock_stream::mock_stream()
	: m_current_action(0), m_action_pos(0)
{
}

void mock_stream::expect_read(yb::buffer_ref const & data, size_t max_chunk_size)
{
	action act;
	act.kind = action::k_read;
	act.data.assign(data.begin(), data.end());
	act.max_chunk_size = max_chunk_size;
	if (m_expected_actions.empty())
		act.ready.set_value();
	m_expected_actions.push_back(act);
}

void mock_stream::expect_write(yb::buffer_ref const & data, size_t max_chunk_size)
{
	action act;
	act.kind = action::k_write;
	act.data.assign(data.begin(), data.end());
	act.max_chunk_size = max_chunk_size;
	if (m_expected_actions.empty())
		act.ready.set_value();
	m_expected_actions.push_back(act);
}

task<size_t> mock_stream::read(uint8_t * buffer, size_t size)
{
	for (size_t i = m_current_action; i < m_expected_actions.size(); ++i)
	{
		if (m_expected_actions[i].kind != action::k_read)
			continue;

		return wait_for(m_expected_actions[i].ready).then([this, i, buffer, size]() -> task<size_t> {
			assert(m_current_action == i);

			action const & act = m_expected_actions[i];
			assert(act.kind == mock_stream::action::k_read);

			size_t chunk = (std::min)(size, act.data.size() - m_action_pos);
			if (act.max_chunk_size && chunk > act.max_chunk_size)
				chunk = act.max_chunk_size;

			std::copy(act.data.data() + m_action_pos, act.data.data() + m_action_pos + chunk, buffer);

			m_action_pos += chunk;
			if (m_action_pos == act.data.size())
			{
				++m_current_action;
				if (m_current_action < m_expected_actions.size())
					m_expected_actions[m_current_action].ready.set_value();
				m_action_pos = 0;
			}

			return async::value(chunk);
		});
	}

	return async::raise<size_t>(std::runtime_error("unexpected action found"));
}

task<size_t> mock_stream::write(uint8_t const * buffer, size_t size)
{
	for (size_t i = m_current_action; i < m_expected_actions.size(); ++i)
	{
		if (m_expected_actions[i].kind != action::k_write)
			continue;

		return wait_for(m_expected_actions[i].ready).then([this, i, buffer, size]() -> task<size_t> {
			assert(m_current_action == i);

			action const & act = m_expected_actions[i];
			assert(act.kind == mock_stream::action::k_write);

			size_t chunk = (std::min)(size, act.data.size() - m_action_pos);
			if (act.max_chunk_size && chunk > act.max_chunk_size)
				chunk = act.max_chunk_size;

			if (!std::equal(buffer, buffer + chunk, act.data.data() + m_action_pos))
				return async::raise<size_t>(std::runtime_error("unexpected data written"));

			m_action_pos += chunk;
			if (m_action_pos == act.data.size())
			{
				++m_current_action;
				if (m_current_action < m_expected_actions.size())
					m_expected_actions[m_current_action].ready.set_value();
				m_action_pos = 0;
			}

			return async::value(chunk);
		});
	}

	return async::raise<size_t>(std::runtime_error("unexpected action found"));
}
