#include "http_request_parser.hpp"

namespace {

struct token_ctx
{
	void start(yb::buffer_ref buf);
	bool stop(std::vector<uint8_t> & token_buf, yb::buffer_ref & token_ref, yb::buffer_ref buf);
	bool stop(std::vector<uint8_t> & token_buf, yb::buffer_ref & token_ref, yb::buffer_ref buf, bool complete);

	uint8_t const * m_buf_start;
};

} // namespace

void token_ctx::start(yb::buffer_ref buf)
{
	m_buf_start = buf.begin();
}

bool token_ctx::stop(std::vector<uint8_t> & token_buf, yb::buffer_ref & token_ref, yb::buffer_ref buf)
{
	return this->stop(token_buf, token_ref, buf, !buf.empty());
}

bool token_ctx::stop(std::vector<uint8_t> & token_buf, yb::buffer_ref & token_ref, yb::buffer_ref buf, bool complete)
{
	if (complete)
	{
		if (token_buf.empty())
		{
			token_ref = yb::buffer_ref(m_buf_start, buf.begin());
		}
		else
		{
			token_buf.insert(token_buf.end(), m_buf_start, buf.begin());
			token_ref = token_buf;
		}
	}
	else
	{
		token_buf.insert(token_buf.end(), m_buf_start, buf.begin());
	}

	return complete;
}

static bool is_token_char(uint8_t ch)
{
	return ch > 0x20 && ch <= 127
		&& ch != '(' && ch != ')' && ch != '<' && ch != '>'
		&& ch != '@' && ch != ',' && ch != ';' && ch != ':'
		&& ch != '\\' && ch != '\"' && ch != '/' && ch != '['
		&& ch != ']' && ch != '?' && ch != '=' && ch != '{'
		&& ch != '}';
}

static bool is_lws_char(uint8_t ch)
{
	return ch == ' ' || ch == '\t';
}

yb::http_request_parser::http_request_parser()
	: m_state(st_method)
{
}

bool yb::http_request_parser::error() const
{
	return m_state == st_error;
}

bool yb::http_request_parser::done() const
{
	return m_state == st_body;
}

yb::buffer_ref yb::http_request_parser::push_data(yb::buffer_ref buf)
{
	while (m_state != st_error && m_state != st_body && !buf.empty())
	{
		switch (m_state)
		{
		case st_method:
			{
				token_ctx ctx;
				ctx.start(buf);
				while (!buf.empty() && is_token_char(buf.front()))
					buf.pop_front();
				if (ctx.stop(m_buffer, m_token, buf))
				{
					m_current_request.method = this->get_token();
					m_state = m_token.empty()? st_error: st_url_sp;
				}
			}
			break;
		case st_url_sp:
			m_state = buf.front() != ' '? st_error: static_cast<state_t>(m_state + 1);
			buf.pop_front();
			break;
		case st_url:
			{
				token_ctx ctx;
				ctx.start(buf);
				while (!buf.empty() && buf.front() != ' ')
					buf.pop_front();
				if (ctx.stop(m_buffer, m_token, buf))
				{
					m_current_request.url = this->get_token();
					m_state = m_token.empty()? st_error: st_version_sp;
				}
			}
			break;
		case st_version_sp:
		case st_version_h:
		case st_version_t1:
		case st_version_t2:
		case st_version_p:
		case st_version_slash:
		case st_version_dot:
		case st_req_cr:
		case st_req_lf:
			{
				static uint8_t const templ[] = { ' ', 'H', 'T', 'T', 'P', '/', 0, '.', 0, '\r', '\n' };
				if (templ[m_state - st_version_sp] != buf.front())
					m_state = st_error;
				else
					m_state = static_cast<state_t>(m_state + 1);
				buf.pop_front();
				break;
			}
			break;
		case st_version_num1:
			if (buf.front() >= '0' && buf.front() < '9')
			{
				m_current_request.version_major = m_current_request.version_major * 10 + (buf.front() - '0');
				buf.pop_front();
			}
			else if (m_current_request.version_major == 0)
				m_state = st_error;
			else
				m_state = st_version_dot;
			break;
		case st_version_num2:
			if (buf.front() >= '0' && buf.front() < '9')
			{
				m_current_request.version_minor = m_current_request.version_minor * 10 + (buf.front() - '0');
				buf.pop_front();
			}
			else if (m_current_request.version_minor == 0)
				m_state = st_error;
			else
				m_state = st_req_cr;
			break;
		case st_header_nl:
			if (is_lws_char(buf.front()))
			{
				m_state = st_header_cont;
				buf.pop_front();
				break;
			}

			if (!m_field_name.empty())
			{
				m_current_request.headers.insert(std::make_pair(m_field_name, m_field_value));
				m_field_name.clear();
				m_field_value.clear();
			}

			if (buf.front() == '\r')
			{
				m_state = st_body_lf;
				buf.pop_front();
				break;
			}

			m_state = st_header_name;
			break;
		case st_header_cont:
			while (!buf.empty() && is_lws_char(buf.front()))
				buf.pop_front();
			if (!buf.empty())
			{
				if (!m_field_value.empty())
					m_field_value.append(' ', 1);
				m_state = st_header_name;
			}
			break;
		case st_header_name:
			{
				token_ctx ctx;
				ctx.start(buf);
				while (!buf.empty() && is_token_char(buf.front()))
					buf.pop_front();
				if (ctx.stop(m_buffer, m_token, buf))
				{
					m_field_name = this->get_lowered_token();
					m_state = m_token.empty()? st_error: st_header_colon;
				}
			}
			break;
		case st_header_colon:
			while (!buf.empty() && is_lws_char(buf.front()))
				buf.pop_front();

			if (!buf.empty() && buf.front() == ':')
			{
				m_state = st_header_value_lws;
				buf.pop_front();
			}
			break;
		case st_header_value_lws:
			while (!buf.empty() && is_lws_char(buf.front()))
				buf.pop_front();
			if (!buf.empty())
			{
				if (!m_field_value.empty())
					m_field_value.append(' ', 1);
				m_state = st_header_value;
			}
			break;
		case st_header_value:
			{
				token_ctx ctx;
				ctx.start(buf);
				while (!buf.empty() && buf.front() != '\r')
					buf.pop_front();
				if (ctx.stop(m_buffer, m_token, buf))
				{
					m_field_value.append(this->get_token());
					m_state = m_token.empty()? st_error: st_header_value_lf;
					buf.pop_front();
				}
			}
			break;
		case st_header_value_lf:
			m_state = buf.front() == '\n'? st_header_nl: st_error;
			buf.pop_front();
			break;
		case st_body_lf:
			m_state = buf.front() == '\n'? st_body: st_error;
			buf.pop_front();
			break;
		}
	}

	return buf;
}

yb::http_request yb::http_request_parser::req()
{
	return std::move(m_current_request);
}

void yb::http_request_parser::clear()
{
	m_current_request.clear();
	m_field_name.clear();
	m_field_value.clear();
	m_buffer.clear();
	m_state = st_method;
}

std::string yb::http_request_parser::get_token() const
{
	std::string res;
	res.resize(m_token.size());
	std::copy(m_token.begin(), m_token.end(), res.begin());
	return res;
}

std::string yb::http_request_parser::get_lowered_token() const
{
	std::string res;
	res.resize(m_token.size());
	for (size_t i = 0; i < m_token.size(); ++i)
	{
		uint8_t ch = m_token[i];
		res[i] = (ch >= 'A' && ch <= 'Z')? ch - 'A' + 'a': ch;
	}
	return res;
}
