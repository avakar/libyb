#include "http.hpp"

yb::http_request::http_request()
	: version_major(0), version_minor(0)
{
}

void yb::http_request::clear()
{
	method.clear();
	url.clear();
	version_major = 0;
	version_minor = 0;
	headers.clear();
}

yb::http_request::field_value_range yb::http_request::header_values(yb::string_ref const & field_name) const
{
	return field_value_range(headers.equal_range(field_name));
}

yb::http_request::field_value_range::field_value_range(std::pair<header_iterator, header_iterator> const & range)
	: m_field_first(range.first), m_field_last(range.second)
{
	if (m_field_first != m_field_last)
	{
		m_front_last = m_field_first->second.data();
		this->find_next();
	}
}

bool yb::http_request::field_value_range::empty() const
{
	return m_field_first == m_field_last;
}

void yb::http_request::field_value_range::pop_front()
{
	assert(m_field_first != m_field_last);

	char const * last = m_field_first->second.data() + m_field_first->second.size();
	if (m_front_last == last)
	{
		++m_field_first;
		if (m_field_first == m_field_last)
			return;

		m_front_last = m_field_first->second.data();
	}
	else
	{
		++m_front_last;
	}

	this->find_next();
}

yb::string_ref yb::http_request::field_value_range::front() const
{
	return yb::string_ref(m_front_first, m_front_last);
}

void yb::http_request::field_value_range::find_next()
{
	char const * last = m_field_first->second.data() + m_field_first->second.size();
	m_front_first = m_front_last;
	while (m_front_last != last)
	{
		if (*m_front_last == ',')
			break;
	}
}

std::string yb::http_response::get_status_text() const
{
	if (!status_text.empty())
		return status_text;

	switch (status_code)
	{
	case 100: return "Continue";
	case 101: return "Switching Protocols";
	case 200: return "OK";
	case 201: return "Created";
	case 202: return "Accepted";
	case 203: return "Non-Authoritative Information";
	case 204: return "No Content";
	case 205: return "Reset Content";
	case 206: return "Partial Content";
	case 300: return "Multiple Choices";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 303: return "See Other";
	case 304: return "Not Modified";
	case 305: return "Temporary Redirect";
	case 307: return "Use Proxy";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 402: return "Payment Required";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 406: return "Not Acceptable";
	case 407: return "Proxy Authentication Required";
	case 408: return "Request Time-out";
	case 409: return "Conflict";
	case 410: return "Gone";
	case 411: return "Length Required";
	case 412: return "Precondition Failed";
	case 413: return "Request Entity Too Large";
	case 414: return "Request-URI Too Large";
	case 415: return "Unsupported Media Type";
	case 416: return "Requested range not satisfiable";
	case 417: return "Expectation Failed";
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Time-out";
	case 505: return "HTTP Version not supported";
	default: return "Unknown";
	}
}
