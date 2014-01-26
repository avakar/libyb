#include "stream.hpp"
#include <vector>
using namespace yb;

task<buffer> yb::read(stream & s, buffer_policy policy)
{
	return s.read(std::move(policy));
}

task<size_t> yb::read(stream & s, uint8_t * buffer, size_t size)
{
	return s.read(buffer_policy(buffer, size)).then([](yb::buffer buf) { return buf.size(); });
}

task<void> yb::read_all(stream & s, uint8_t * buffer, size_t size)
{
	return loop_with_state<size_t, size_t>(async::value((size_t)0), 0, [&s, buffer, size](size_t r, size_t & st, cancel_level cl) -> task<size_t> {
		st += r;
		if (cl >= cl_abort)
			return async::raise<size_t>(task_cancelled(cl));
		if (st == size)
			return nulltask;
		return read(s, buffer + st, size - st);
	});
}

task<void> yb::read_all(stream & s, std::function<bool(buffer_ref const & chunk)> const & handler, size_t max_chunk_size)
{
	if (max_chunk_size == 0)
		max_chunk_size = 16*1024;
	size_t const buffer_size = (std::min)(max_chunk_size, (size_t)(16 * 1024));
	std::shared_ptr<std::vector<uint8_t> > buf = std::make_shared<std::vector<uint8_t> >(buffer_size);
	return loop(async::value((size_t)-1), [&s, handler, buf](size_t r, cancel_level cl) -> task<size_t> {
		if (cl >= cl_quit)
			return async::raise<size_t>(task_cancelled(cl));
		if (r != (size_t)-1 && !handler(buffer_ref(buf->data(), r)))
			return nulltask;
		return read(s, buf->data(), buf->size());
	});
}

task<size_t> yb::write(stream & s, buffer_ref buf)
{
	return s.write(buf);
}

task<size_t> yb::write(stream & s, uint8_t const * buffer, size_t size)
{
	return s.write(buffer_ref(buffer, size));
}

task<void> yb::write_all(stream & s, uint8_t const * buffer, size_t size)
{
	return loop_with_state<size_t, size_t>(async::value((size_t)0), 0, [&s, buffer, size](size_t r, size_t & st, cancel_level cl) -> task<size_t> {
		st += r;
		if (cl >= cl_abort)
			return async::raise<size_t>(task_cancelled(cl));
		if (st == size)
			return nulltask;
		return write(s, buffer + st, size - st);
	});
}

static task<void> copy_iter(std::shared_ptr<std::vector<uint8_t>> const & ctx, stream & sink, stream & source)
{
	return read(source, ctx->data(), ctx->size()).abort_on(cl_quit).then([ctx, &sink](size_t r) {
		return write_all(sink, ctx->data(), r);
	});
}

task<void> yb::copy(stream & sink, stream & source, size_t buffer_size)
{
	try
	{
		std::shared_ptr<std::vector<uint8_t>> ctx(new std::vector<uint8_t>(buffer_size));
		return loop([ctx, &sink, &source](cancel_level cl) {
			return cl >= cl_quit? nulltask: copy_iter(ctx, sink, source);
		});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}

task<void> yb::discard(stream & source, size_t buffer_size)
{
	try
	{
		std::shared_ptr<std::vector<uint8_t>> ctx(new std::vector<uint8_t>(buffer_size));
		return loop([ctx, &source](cancel_level cl) {
			return cl >= cl_quit? nulltask: read(source, ctx->data(), ctx->size()).abort_on(cl_quit).ignore_result();
		});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}
