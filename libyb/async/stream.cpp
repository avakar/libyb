#include "stream.hpp"
#include <vector>
using namespace yb;

task<void> stream::read_all(uint8_t * buffer, size_t size)
{
	return loop_with_state<size_t, size_t>(async::value((size_t)0), 0, [this, buffer, size](size_t r, size_t & st, cancel_level cl) -> task<size_t> {
		st += r;
		if (cl >= cl_abort)
			return async::raise<size_t>(task_cancelled(cl));
		if (st == size)
			return nulltask;
		return this->read(buffer + st, size - st);
	});
}

task<void> stream::write_all(uint8_t const * buffer, size_t size)
{
	return loop_with_state<size_t, size_t>(async::value((size_t)0), 0, [this, buffer, size](size_t r, size_t & st, cancel_level cl) -> task<size_t> {
		st += r;
		if (cl >= cl_abort)
			return async::raise<size_t>(task_cancelled(cl));
		if (st == size)
			return nulltask;
		return this->write(buffer + st, size - st);
	});
}

static task<void> copy_iter(std::shared_ptr<std::vector<uint8_t>> const & ctx, stream & sink, stream & source)
{
	return source.read(ctx->data(), ctx->size()).abort_on(cl_quit).then([ctx, &sink](size_t r) {
		return sink.write_all(ctx->data(), r);
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
			return cl >= cl_quit? nulltask: source.read(ctx->data(), ctx->size()).abort_on(cl_quit).ignore_result();
		});
	}
	catch (...)
	{
		return async::raise<void>();
	}
}
