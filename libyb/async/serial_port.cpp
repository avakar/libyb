#include "serial_port.hpp"
#include "detail/win32_handle_task.hpp"
#include <windows.h>
using namespace yb;

struct serial_port::impl
{
	HANDLE hFile;
	OVERLAPPED readOverlapped;
	OVERLAPPED writeOverlapped;
};

serial_port::serial_port()
	: m_pimpl(new impl())
{
	m_pimpl->hFile = INVALID_HANDLE_VALUE;

	m_pimpl->readOverlapped.hEvent = CreateEvent(0, TRUE, FALSE, 0);
	if (!m_pimpl->readOverlapped.hEvent)
		throw std::runtime_error("failed to allocate event");

	m_pimpl->writeOverlapped.hEvent = CreateEvent(0, TRUE, FALSE, 0);
	if (!m_pimpl->writeOverlapped.hEvent)
	{
		CloseHandle(m_pimpl->readOverlapped.hEvent);
		throw std::runtime_error("failed to allocate event");
	}
}

serial_port::~serial_port()
{
	this->close();
	CloseHandle(m_pimpl->writeOverlapped.hEvent);
	CloseHandle(m_pimpl->readOverlapped.hEvent);
}

void serial_port::close()
{
	if (m_pimpl->hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_pimpl->hFile);
		m_pimpl->hFile = INVALID_HANDLE_VALUE;
	}
}

struct open_thread_params
{
	explicit open_thread_params()
		: m_dcb(), m_hFile(INVALID_HANDLE_VALUE), m_state(st_running)
	{
		InitializeCriticalSection(&cs);
	}

	~open_thread_params()
	{
		DeleteCriticalSection(&cs);
	}

	CRITICAL_SECTION cs;
	std::string m_name;
	DCB m_dcb;

	HANDLE m_hFile;
	DWORD dwError;

	enum state_t { st_running, st_finished, st_cancelled };
	state_t m_state;

	open_thread_params(open_thread_params const &);
	open_thread_params & operator=(open_thread_params const &);
};

static DWORD CALLBACK open_thread(void * param)
{
	open_thread_params * params = (open_thread_params *)param;

	HANDLE hFile = CreateFileA(params->m_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

	SetCommState(hFile, &params->m_dcb);

	COMMTIMEOUTS ct = {};
	ct.ReadIntervalTimeout = MAXDWORD;
	ct.ReadTotalTimeoutConstant = MAXDWORD - 1;
	ct.ReadTotalTimeoutMultiplier = MAXDWORD;
	SetCommTimeouts(hFile, &ct);

	bool delete_params = false;
	EnterCriticalSection(&params->cs);
	if (params->m_state == open_thread_params::st_cancelled)
	{
		delete_params = true;
		if (hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);
	}
	else
	{
		params->m_hFile = hFile;
		params->dwError = GetLastError();
		params->m_state = open_thread_params::st_finished;
	}
	LeaveCriticalSection(&params->cs);

	if (delete_params)
		delete params;

	return 0;
}

task<void> serial_port::open(string_ref const & name, int baudrate)
{
	settings s;
	s.baudrate = baudrate;
	return this->open(name, s);
}

task<void> serial_port::open(string_ref const & name, settings const & s)
{
	try
	{
		std::unique_ptr<open_thread_params> params(new open_thread_params());
		params->m_name = name;

		params->m_dcb.DCBlength = sizeof(DCB);
		params->m_dcb.BaudRate = s.baudrate;
		params->m_dcb.ByteSize = 8;
		params->m_dcb.fBinary = TRUE;

		DWORD dwThreadId;
		HANDLE hThread = CreateThread(0, 0, &open_thread, params.get(), 0, &dwThreadId);
		if (!hThread)
			return std::copy_exception(std::runtime_error("failed to create the open thread"));

		open_thread_params * params2 = params.release();

		task<void> t = make_win32_handle_task(hThread, [hThread, params2](cancel_level cl) -> bool {
			if (cl < cl_abort)
				return true;

			bool cancelled = false;
			EnterCriticalSection(&params2->cs);
			if (params2->m_state == open_thread_params::st_running)
			{
				if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
				{
					typedef BOOL WINAPI CancelSynchronousIo_t(HANDLE hThread);
					if (CancelSynchronousIo_t * CancelSynchronousIo = (CancelSynchronousIo_t *)GetProcAddress(hKernel32, "CancelSynchronousIo"))
						CancelSynchronousIo(hThread);
				}
				params2->m_state = open_thread_params::st_cancelled;
				cancelled = true;
			}
			LeaveCriticalSection(&params2->cs);

			if (cancelled)
				CloseHandle(hThread);

			return !cancelled;
		}).then([this, hThread, params2]() -> task<void> {
			CloseHandle(hThread);
			m_pimpl->hFile = params2->m_hFile;
			//DWORD dwError = params2->dwError;
			delete params2;

			if (m_pimpl->hFile == INVALID_HANDLE_VALUE)
				return async::raise<void>(std::runtime_error("error opening the device"));
			return async::value();
		});

		return std::move(t);
	}
	catch (...)
	{
		return std::current_exception();
	}
}

task<size_t> serial_port::read(uint8_t * buffer, size_t size)
{
	assert(m_pimpl->hFile);

	DWORD dwTransferred;
	if (!ReadFile(m_pimpl->hFile, buffer, size, &dwTransferred, &m_pimpl->readOverlapped))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			return make_win32_handle_task(m_pimpl->readOverlapped.hEvent, [this](cancel_level cl) -> bool {
				if (cl < cl_abort)
					return true;

				if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
				{
					typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
					if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
						CancelIoEx(m_pimpl->hFile, &m_pimpl->readOverlapped);
					else
						CancelIo(m_pimpl->hFile);
				}
				else
				{
					CancelIo(m_pimpl->hFile);
				}

				return true;
			}).then([this]() -> task<size_t> {
				DWORD dwTransferred;
				if (!GetOverlappedResult(m_pimpl->hFile, &m_pimpl->readOverlapped, &dwTransferred, TRUE))
					throw std::runtime_error("the read operation failed"); // XXX
				return async::value((size_t)dwTransferred);
			});
		}
		else
			return async::raise<size_t>(std::runtime_error("the read operation failed")); // XXX
	}
	else
	{
		return async::value((size_t)dwTransferred);
	}
}

task<size_t> serial_port::write(uint8_t const * buffer, size_t size)
{
	assert(m_pimpl->hFile);

	DWORD dwTransferred;
	if (!WriteFile(m_pimpl->hFile, buffer, size, &dwTransferred, &m_pimpl->writeOverlapped))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			return make_win32_handle_task(m_pimpl->writeOverlapped.hEvent, [this](cancel_level cl) -> bool {
				if (cl < cl_abort)
					return true;

				if (HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll"))
				{
					typedef BOOL WINAPI CancelIoEx_t(HANDLE hFile, LPOVERLAPPED lpOverlapped);
					if (CancelIoEx_t * CancelIoEx = (CancelIoEx_t *)GetProcAddress(hKernel32, "CancelIoEx"))
						CancelIoEx(m_pimpl->hFile, &m_pimpl->writeOverlapped);
					else
						CancelIo(m_pimpl->hFile);
				}
				else
				{
					CancelIo(m_pimpl->hFile);
				}

				return true;
			}).then([this]() -> task<size_t> {
				DWORD dwTransferred;
				if (!GetOverlappedResult(m_pimpl->hFile, &m_pimpl->writeOverlapped, &dwTransferred, TRUE))
					throw std::runtime_error("the read operation failed"); // XXX
				return async::value((size_t)dwTransferred);
			});
		}
		else
			return async::raise<size_t>(std::runtime_error("the read operation failed")); // XXX
	}
	else
	{
		return async::value((size_t)dwTransferred);
	}
}
