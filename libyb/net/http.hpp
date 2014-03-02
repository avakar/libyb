#ifndef LIBYB_NET_HTTP_HPP
#define LIBYB_NET_HTTP_HPP

#include "../async/stream.hpp"
#include <string>
#include <map>
#include <memory>

namespace yb {

struct http_request
{
	std::string method;
	std::string url;
	int version_major;
	int version_minor;
	std::multimap<std::string, std::string> headers;
	std::shared_ptr<stream> body;

	class field_value_range
	{
	public:
		typedef std::multimap<std::string, std::string>::const_iterator header_iterator;

		field_value_range(std::pair<header_iterator, header_iterator> const & range);
		bool empty() const;
		void pop_front();
		yb::string_ref front() const;

	private:
		void find_next();

		header_iterator m_field_first;
		header_iterator m_field_last;
		char const * m_front_first;
		char const * m_front_last;
	};

	http_request();
	void clear();
	field_value_range header_values(yb::string_ref const & field_name) const;
};

struct http_response
{
	int status_code;
	std::string status_text;
	std::multimap<std::string, std::string> headers;
	std::shared_ptr<stream> body;

	std::string get_status_text() const;
};

} // namespace yb

#endif // LIBYB_NET_HTTP_HPP
