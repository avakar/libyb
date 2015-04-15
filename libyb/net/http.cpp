#include "http.hpp"

std::string yb::http_headers::get(string_ref field_name) const
{
	std::string res;
	if (!try_get(field_name, res))
		throw http_error::invalid_request();
	return res;
}

std::string yb::http_headers::get(string_ref field_name, string_ref def) const
{
	std::string res;
	if (!try_get(field_name, res))
		res = def;
	return res;
}

bool yb::http_headers::contains(string_ref field_name) const
{
	return m_headers.find(field_name) != m_headers.end();
}

bool yb::http_headers::try_get(string_ref field_name, std::string & field_value) const
{
	auto range = m_headers.equal_range(field_name);
	if (range.first == range.second)
		return false;
	if (std::next(range.first) != range.second)
		throw http_error::invalid_request();

	field_value = range.first->second;
	return true;
}

yb::http_headers::header_range yb::http_headers::get_all() const
{
	return header_range(m_headers.begin(), m_headers.end());
}

yb::http_headers::header_range yb::http_headers::get_all(string_ref field_name) const
{
	auto r = m_headers.equal_range(field_name);
	return header_range(r.first, r.second);
}

yb::range<yb::http_headers::split_value_range> yb::http_headers::get_split(string_ref field_name) const
{
	return yb::range<yb::http_headers::split_value_range>(this->get_all(field_name));
}

std::string yb::http_headers::get_combined(string_ref field_name) const
{
	std::string res;
	for (auto range = m_headers.equal_range(field_name); range.first != range.second; ++range.first)
	{
		if (!res.empty())
			res.append(", ");
		res.append(range.first->second);
	}
	return res;
}

void yb::http_headers::add(string_ref field_name, string_ref field_value)
{
	m_headers.insert(std::make_pair(field_name, field_value));
}

void yb::http_headers::clear()
{
	m_headers.clear();
}

yb::http_headers::split_value_range::split_value_range(header_range range)
	: m_header_range(std::move(range))
{
	if (!m_header_range.empty())
	{
		m_front_last = m_header_range.front().second.data();
		this->find_next();
	}
}

bool yb::http_headers::split_value_range::empty() const
{
	return m_header_range.empty();
}

void yb::http_headers::split_value_range::pop_front()
{
	assert(!this->empty());

	auto const & header = m_header_range.front();
	char const * last = header.second.data() + header.second.size();
	if (m_front_last == last)
	{
		m_header_range.pop_front();
		if (m_header_range.empty())
			return;

		m_front_last = m_header_range.front().second.data();
	}
	else
	{
		++m_front_last;
	}

	this->find_next();
}

yb::string_ref yb::http_headers::split_value_range::front() const
{
	return yb::string_ref(m_front_first, m_front_last);
}

void yb::http_headers::split_value_range::find_next()
{
	auto const & header = m_header_range.front();
	char const * last = header.second.data() + header.second.size();
	m_front_first = m_front_last;
	while (m_front_last != last)
	{
		if (*m_front_last == ',')
			break;
	}
}

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
