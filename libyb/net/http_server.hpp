#ifndef LIBYB_NET_HTTP_SERVER_HPP
#define LIBYB_NET_HTTP_SERVER_HPP

#include "http.hpp"
#include "../async/stream.hpp"
#include <functional>

namespace yb {

task<void> run_http_server(stream & s, std::function<task<http_response>(http_request const &)> fn);

} // namespace yb

#endif // LIBYB_NET_HTTP_SERVER_HPP
