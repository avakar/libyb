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

task<size_t> console_stream::read(uint8_t * buffer, size_t size)
{
	return try_read(m_pimpl->hStdin, buffer, size).then([this, buffer, size](size_t r) -> task<size_t> {
		if (r == 0)
			return this->read(buffer, size); // XXX recursion!
		return async::value(r);
	});
}

task<size_t> console_stream::write(uint8_t const * buffer, size_t size)
{
	std::shared_ptr<detail::win32_file_operation> fo = std::make_shared<detail::win32_file_operation>();
	return fo->write(m_pimpl->hStdout, buffer, size).keep_alive(fo);
}
