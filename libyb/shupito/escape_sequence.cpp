#include "escape_sequence.hpp"
#include "../async/timer.hpp"
using namespace yb;

struct send_escape_context
{
	send_escape_context(stream & sp, int cycle_length_ms, uint8_t const * escseq, size_t escseq_len, uint8_t const * respseq, size_t respseq_len)
		: sp(sp), cycle_length_ms(cycle_length_ms), escseq(escseq), escseq_len(escseq_len), respseq(respseq), respseq_len(respseq_len), respptr(respseq)
	{
	}

	timer t;
	stream & sp;
	int cycle_length_ms;
	uint8_t const * escseq;
	size_t escseq_len;

	uint8_t const * respseq;
	size_t respseq_len;
	uint8_t const * respptr;

	uint8_t read_buf[16];
	cancellation_token read_cancel;
};

static task<void> send_escape_seq_write_iter(std::shared_ptr<send_escape_context> const & ctx, cancel_level cl)
{
	if (cl >= cl_abort)
		return nulltask;

	if (ctx->respptr == ctx->respseq + ctx->respseq_len)
	{
		ctx->read_cancel.cancel(cl_abort);
		return nulltask;
	}

	ctx->respptr = ctx->respseq;

	return ctx->sp.write_all(ctx->escseq, ctx->escseq_len).then([ctx]{
		return ctx->t.wait_ms(ctx->cycle_length_ms);
	});
}

task<void> yb::send_generic_escape_seq(stream & sp, int cycle_length_ms, uint8_t const * escseq, size_t escseq_len, uint8_t const * respseq, size_t respseq_len)
{
	std::shared_ptr<send_escape_context> ctx = std::shared_ptr<send_escape_context>(new send_escape_context(sp, cycle_length_ms, escseq, escseq_len, respseq, respseq_len));

	task<void> read_task = loop<size_t>(async::value((size_t)0), [ctx](size_t r, cancel_level cl) -> task<size_t> {
		if (ctx->respptr)
		{
			for (size_t i = 0; i < r && size_t(ctx->respptr - ctx->respseq) < ctx->respseq_len; ++i)
			{
				if (*ctx->respptr++ != ctx->read_buf[i])
				{
					ctx->respptr = 0;
					break;
				}
			}
		}
		return cl >= cl_abort? nulltask: ctx->sp.read(ctx->read_buf, sizeof ctx->read_buf);
	}).finishable(ctx->read_cancel);

	task<void> write_task = loop([ctx](cancel_level cl) {
		return send_escape_seq_write_iter(ctx, cl);
	});

	return std::move(write_task) | std::move(read_task);
}

task<void> yb::send_avr232boot_escape_seq(stream & sp, int cycle_length_ms)
{
	static uint8_t const escape_seq[] = { 't', '~', 'z', '3' };
	static uint8_t const response[] = { 20 };
	return send_generic_escape_seq(sp, cycle_length_ms, escape_seq, sizeof escape_seq, response, sizeof response);
}
