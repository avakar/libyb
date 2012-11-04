#ifndef LIBYB_ASYNC_CANCELLATION_TOKEN_HPP
#define LIBYB_ASYNC_CANCELLATION_TOKEN_HPP

#include "cancel_level.hpp"

namespace yb {

namespace detail {
class cancellation_token_core_base;
}

class cancellation_token
{
public:
	cancellation_token();
	explicit cancellation_token(detail::cancellation_token_core_base * core);
	cancellation_token(cancellation_token const & o);
	~cancellation_token();
	cancellation_token & operator=(cancellation_token const & o);
	void cancel(cancel_level cl);

private:
	detail::cancellation_token_core_base * m_core;
};

} // namespace yb

#endif // LIBYB_ASYNC_CANCELLATION_TOKEN_HPP
