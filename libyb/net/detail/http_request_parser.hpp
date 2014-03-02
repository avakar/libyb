#ifndef LIBYB_HTTP_REQUEST_PARSER_HPP
#define LIBYB_HTTP_REQUEST_PARSER_HPP

#include "../http.hpp"
#include "../../vector_ref.hpp"

namespace yb {

class http_request_parser
{
public:
	http_request_parser();

	bool error() const;
	bool done() const;
	yb::buffer_ref push_data(yb::buffer_ref buf);
	http_request req();
	void clear();

private:
	enum state_t {
		st_method, st_url_sp, st_url,
		st_version_sp, st_version_h, st_version_t1, st_version_t2, st_version_p, st_version_slash, st_version_num1, st_version_dot, st_version_num2, st_req_cr, st_req_lf,
		st_header_name, st_header_colon, st_header_value_lws, st_header_value, st_header_value_cr, st_header_value_lf, st_header_nl, st_header_cont,
		st_body_lf, st_body,
		st_error
	} m_state;

	std::string get_token() const;
	std::string get_lowered_token() const;

	std::vector<uint8_t> m_buffer;
	yb::buffer_ref m_token;
	std::string m_field_name, m_field_value;

	http_request m_current_request;
};

} // namespace yb

#endif // LIBYB_HTTP_REQUEST_PARSER_HPP
