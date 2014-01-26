#include "../console_stream.hpp"
#include "win32_handle_task.hpp"
#include "../../utils/detail/win32_file_operation.hpp"
#include <windows.h>
using namespace yb;

struct console_stream::impl
{
	HANDLE hStdin;
	HANDLE hStdout;
};

console_stream::console_stream(output_kind_t output_kind)
	: m_pimpl(0)
{
	std::unique_ptr<impl> pimpl(new impl());
	pimpl->hStdin = GetStdHandle(STD_INPUT_HANDLE);
	pimpl->hStdout = GetStdHandle(output_kind == out? STD_OUTPUT_HANDLE: STD_ERROR_HANDLE);
	m_pimpl = pimpl.release();
}

console_stream::~console_stream()
{
	delete m_pimpl;
}

static task<size_t> try_read(HANDLE hFile, uint8_t * buffer, size_t size)
{
	return yb::make_win32_handle_task(hFile, [](cancel_level cl) {
		return false;
	}).then([hFile, buffer, size]() -> task<size_t> {
		INPUT_RECORD ir[16];
		DWORD dwRet;
		ReadConsoleInputA(hFile, ir, (std::min)((size_t)16, size), &dwRet);

		size_t res = 0;
		for (DWORD i = 0; i < dwRet; ++i)
		{
			if (ir[i].EventType != KEY_EVENT)
				continue;
			if (ir[i].Event.KeyEvent.uChar.AsciiChar == 0 || !ir[i].Event.KeyEvent.bKeyDown)
				continue;
			buffer[res++] = ir[i].Event.KeyEvent.uChar.AsciiChar;
		}

		return async::value(res);
	});
}

task<buffer> console_stream::read(buffer_policy policy)
{
	struct th
	{
		th(th && o)
			: m_self(o.m_self), m_buf(std::move(o.m_buf))
		{
		}

		th & operator=(th && o)
		{
			m_self = o.m_self;
			m_buf = std::move(o.m_buf);
			return *this;
		}

		th(console_stream * self, buffer && buf)
			: m_self(self), m_buf(std::move(buf))
		{
		}

		task<buffer> operator()(size_t r)
		{
			if (r == 0)
				return m_self->read(std::move(m_buf)); // XXX recursion!
			m_buf.shrink(r);
			return async::value(std::move(m_buf));
		}

		console_stream * m_self;
		buffer m_buf;
	};

	return policy.fetch().then([this](buffer buf) {
		th t(this, std::move(buf));
		task<size_t> res = try_read(m_pimpl->hStdin, t.m_buf.get(), t.m_buf.size());
		return res.then(std::move(t));
	});
}

task<size_t> console_stream::write(buffer_ref buf)
{
	std::shared_ptr<detail::win32_file_operation> fo = std::make_shared<detail::win32_file_operation>();
	return fo->write(m_pimpl->hStdout, buf.data(), buf.size()).keep_alive(fo);
}
