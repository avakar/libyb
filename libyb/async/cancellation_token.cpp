#include "cancellation_token.hpp"
#include "detail/cancellation_token_task.hpp"
using namespace yb;

cancellation_token::cancellation_token()
	: m_core(0)
{
}

cancellation_token::cancellation_token(detail::cancellation_token_core_base * core)
	: m_core(core)
{
	assert(m_core);
	m_core->addref();
}

cancellation_token::cancellation_token(cancellation_token const & o)
	: m_core(o.m_core)
{
	if (m_core)
		m_core->addref();
}

cancellation_token::~cancellation_token()
{
	if (m_core)
		m_core->release();
}

cancellation_token & cancellation_token::operator=(cancellation_token const & o)
{
	if (o.m_core)
		o.m_core->addref();
	if (m_core)
		m_core->release();
	m_core = o.m_core;
	return *this;
}

void cancellation_token::cancel(cancel_level cl)
{
	if (m_core)
		m_core->cancel(cl);
}
