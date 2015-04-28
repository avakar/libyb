#ifndef LIBYB_NET_HTTP_HPP
#define LIBYB_NET_HTTP_HPP

#include "../utils/range.hpp"
#include "../async/stream.hpp"
#include "../vector_ref.hpp"
#include <string>
#include <map>
#include <memory>

namespace yb {

struct http_error
	: std::runtime_error
{
	int status_code;

	explicit http_error(int status_code)
		: std::runtime_error("http_error"), status_code(status_code)
	{
	}

	static http_error invalid_request()
	{
		return http_error(400);
	}
};

struct http_header_comparator
{
	bool operator()(string_ref lhs, string_ref rhs) const
	{
		auto tolower = [](char ch) { return 'A' <= ch && ch <= 'Z' ? ch + ('a' - 'A') : ch; };

		for (size_t i = 0; i < lhs.size() && i < rhs.size(); ++i)
		{
			int cmp = tolower(lhs[i]) - tolower(rhs[i]);
			if (cmp < 0)
				return true;
			if (cmp > 0)
				return false;
		}

		return lhs.size() < rhs.size();
	}
};

class http_headers
{
public:
	typedef range_adaptor<std::multimap<std::string, std::string, http_header_comparator>::const_iterator> header_range;
	class split_value_range
	{
	public:
		typedef string_ref value_type;
		typedef string_ref reference;

		split_value_range(header_range range);
		bool empty() const;
		void pop_front();
		yb::string_ref front() const;

	private:
		void find_next();

		header_range m_header_range;
		char const * m_front_first;
		char const * m_front_last;
	};

	std::string get(string_ref field_name) const;
	std::string get(string_ref field_name, string_ref def) const;
	bool contains(string_ref field_name) const;
	bool try_get(string_ref field_name, std::string & field_value) const;

	header_range get_all() const;
	header_range get_all(string_ref field_name) const;

	range<split_value_range> get_split(string_ref field_name) const;
	std::string get_combined(string_ref field_name) const;

	void add(string_ref field_name, string_ref field_value);

	void clear();

private:
	std::multimap<std::string, std::string, http_header_comparator> m_headers;
};

struct http_request
{
	std::string method;
	std::string url;
	int version_major;
	int version_minor;
	http_headers headers;
	std::shared_ptr<istream> body;

	http_request();
	void clear();
};

struct http_response
{
	int status_code;
	std::string status_text;
	http_headers headers;
	std::shared_ptr<istream> body;

	std::string get_status_text() const;
};

} // namespace yb

#endif // LIBYB_NET_HTTP_HPP
