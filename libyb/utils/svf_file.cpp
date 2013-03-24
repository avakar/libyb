#include "svf_file.hpp"
#include <sstream>
#include <cassert>
#include <algorithm>
using namespace yb;

namespace {

struct parse_context
{
	std::streambuf * fin;
	svf_file stmts;
};

}

static bool is_final_state(svf_state s)
{
	return s == tap_reset || s == tap_idle || s == tap_drpause || s == tap_irpause;
}

static void parse_whitespace(parse_context & ctx)
{
	enum { normal, slash, comment } state = normal;
	std::unique_ptr<svf_comment> stmt(new svf_comment());

	for (int ch = ctx.fin->sgetc(); ch != EOF; ch = ctx.fin->snextc())
	{
		switch (state)
		{
		case normal:
			if (ch == '!')
			{
				stmt->text.push_back('!');
				stmt->comment_kind = svf_comment::ck_bang;
				state = comment;
			}
			else if (ch == '/')
				state = slash;
			else if (!isspace(ch))
				return;
			break;
		case slash:
			if (ch == '/')
			{
				stmt->text.append("//");
				stmt->comment_kind = svf_comment::ck_slashslash;
				state = comment;
			}
			else
			{
				ctx.fin->sungetc();
				return;
			}
			break;
		case comment:
			if (ch == '\n')
			{
				state = normal;
				ctx.stmts.push_back(std::move(stmt));
				stmt.reset(new svf_comment());
			}
			else
			{
				stmt->text.push_back((char)ch);
			}
			break;
		}
	}

	if (state == comment)
		ctx.stmts.push_back(std::move(stmt));
}

static std::string parse_ident(parse_context & ctx)
{
	parse_whitespace(ctx);

	std::string res;
	int ch;
	for (ch = ctx.fin->sgetc(); ch != EOF && isalpha(ch); ch = ctx.fin->snextc())
		res.push_back((char)ch);

	if (ch != EOF && res.empty())
		throw std::runtime_error("expected an identifier");

	return res;
}

static std::string upper(std::string s)
{
	for (size_t i = 0; i < s.size(); ++i)
		s[i] = toupper((unsigned char)s[i]);
	return std::move(s);
}

static bool try_parse_semicolon(parse_context & ctx)
{
	parse_whitespace(ctx);
	if (ctx.fin->sgetc() != ';')
		return false;
	ctx.fin->sbumpc();
	return true;
}

static void parse_semicolon(parse_context & ctx)
{
	if (!try_parse_semicolon(ctx))
		throw std::runtime_error("expected a semicolon");
}

static uint32_t parse_uint32(parse_context & ctx)
{
	parse_whitespace(ctx);

	bool ok = false;
	uint32_t res = 0;
	for (int ch = ctx.fin->sgetc(); ch != EOF; ch = ctx.fin->snextc())
	{
		if (ch >= '0' && ch <= '9')
			res = res * 10 + (ch - '0');
		else if (!ok)
			throw std::runtime_error("svf: expected an integer");
		else
			break;

		ok = true;
	}

	return res;
}

static svf_state parse_tap_state(parse_context & ctx)
{
	std::string state_name = parse_ident(ctx);
	state_name = upper(std::move(state_name));

	struct
	{
		char const * name;
		svf_state state;
	} static const state_names[] = {
		{ "RESET", tap_reset, },
		{ "IDLE", tap_idle, },
		{ "DRSELECT", tap_drselect, },
		{ "DRCAPTURE", tap_drcapture, },
		{ "DRSHIFT", tap_drshift, },
		{ "DREXIT1", tap_drexit1, },
		{ "DRPAUSE", tap_drpause, },
		{ "DREXIT2", tap_drexit2, },
		{ "DRUPDATE", tap_drupdate, },
		{ "IRSELECT", tap_irselect, },
		{ "IRCAPTURE", tap_ircapture, },
		{ "IRSHIFT", tap_irshift, },
		{ "IREXIT", tap_irexit1, },
		{ "IRPAUSE", tap_irpause, },
		{ "IREXIT2", tap_irexit2, },
		{ "IRUPDATE", tap_irupdate, },
	};

	for (size_t i = 0; i < sizeof state_names / sizeof state_names[0]; ++i)
	{
		if (state_name == state_names[i].name)
			return state_names[i].state;
	}

	return tap_unknown;
}

static double parse_real(parse_context & ctx)
{
	parse_whitespace(ctx);

	std::string res;
	enum { first, before_dot, dot, after_dot, exp_sign, exp_first_num, exp_num, done } state = first;

	for (int ch = ctx.fin->sgetc(); ch != EOF; ch = ctx.fin->snextc())
	{
		switch (state)
		{
		case first:
			if (!isdigit(ch))
				throw std::runtime_error("svf: expected a digit");
			state = before_dot;
			break;
		case before_dot:
			if (ch == '.')
				state = dot;
			else if (ch == 'E')
				state = exp_sign;
			else if (!isdigit(ch))
				state = done;
			break;
		case dot:
			if (!isdigit(ch))
				throw std::runtime_error("svf: a decimal point must be followed by a digit");
			state = after_dot;
			break;
		case after_dot:
			if (ch == 'E')
				state = exp_sign;
			else if (!isdigit(ch))
				state = done;
			break;
		case exp_sign:
			if (ch == '+' || ch == '-')
				state = exp_first_num;
			else if (isdigit(ch))
				state = exp_num;
			else
				throw std::runtime_error("svf: invalid real number format");
			break;
		case exp_first_num:
			if (isdigit(ch))
				state = exp_num;
			else
				throw std::runtime_error("svf: invalid real number format");
			break;
		case exp_num:
			if (!isdigit(ch))
				state = done;
			break;
		}

		if (state == done)
			break;

		res.push_back(ch);
	}

	std::stringstream ss(std::move(res));
	double dres;
	if (!(ss >> dres))
		throw std::runtime_error("svf: invalid real number format");
	return dres;
}

static std::vector<uint8_t> parse_vector(parse_context & ctx, size_t size)
{
	parse_whitespace(ctx);
	if (ctx.fin->sbumpc() != '(')
		throw std::runtime_error("svf: expected '(' as the left vector delimiter");

	std::vector<uint8_t> res;
	res.reserve((size + 7) / 4);
	for (int ch = ctx.fin->sgetc(); ch != ')'; ch = ctx.fin->snextc())
	{
		uint8_t v;
		if (ch >= '0' && ch <= '9')
			v = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			v = ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			v = ch - 'A' + 10;
		else if (isspace(ch))
			continue;
		else
			throw std::runtime_error("svf: malformed vector encountered");

		res.push_back(v);
	}
	ctx.fin->sbumpc();

	std::reverse(res.begin(), res.end());
	if (res.size() % 2)
		res.push_back(0);

	for (size_t i = 0; i < res.size(); i += 2)
		res[i/2] = res[i] | (res[i+1] << 4);

	res.resize(res.size() / 2);
	res.resize((size + 7) / 8);
	return res;
}

static std::unique_ptr<svf_runtest> parse_runtest(parse_context & ctx)
{
	std::unique_ptr<svf_runtest> stmt(new svf_runtest());

	try
	{
		stmt->run_state = parse_tap_state(ctx);
	}
	catch (std::runtime_error const &)
	{
	}

	if (stmt->run_state != tap_unknown && !is_final_state(stmt->run_state))
		throw std::runtime_error("svf: expected a stable state");

	double run_count_or_min_time = parse_real(ctx);
	std::string run_clk = parse_ident(ctx);

	bool has_mintime = false;

	if (run_clk == "TCK" || run_clk == "SCK")
	{
		stmt->run_clk = run_clk == "TCK"? svf_runtest::tck: svf_runtest::sck;
		stmt->run_count = (uint32_t)run_count_or_min_time;
	}
	else if (run_clk == "SEC")
	{
		stmt->min_time = run_count_or_min_time;
		has_mintime = true;
	}
	else
	{
		throw std::runtime_error("svf: expected TCK, SCK or SEC.");
	}

	while (!try_parse_semicolon(ctx))
	{
		if (!has_mintime)
		{
			std::streampos pos = ctx.fin->pubseekoff(0, std::ios::cur, std::ios::in);
			try
			{
				stmt->min_time = parse_real(ctx);
				has_mintime = true;
			}
			catch (std::runtime_error const &)
			{
				ctx.fin->pubseekpos(pos, std::ios::in);
			}

			if (has_mintime)
			{
				if (upper(parse_ident(ctx)) != "SEC")
					throw std::runtime_error("svf: expected SEC");
				continue;
			}
		}

		has_mintime = true;

		std::string arg = upper(parse_ident(ctx));
		if (arg == "MAXIMUM")
		{
			stmt->max_time = parse_real(ctx);
			if (upper(parse_ident(ctx)) != "SEC")
				throw std::runtime_error("svf: expected SEC");
		}
		else if (arg == "ENDSTATE")
		{
			stmt->end_state = parse_tap_state(ctx);
			if (!is_final_state(stmt->end_state))
				throw std::runtime_error("svf: expected a stable state");
		}
	}

	return stmt;
}

static void parse_xxr(parse_context & ctx, svf_statement_kind kind)
{
	std::unique_ptr<svf_xxr> stmt(new svf_xxr(kind));
	stmt->length = parse_uint32(ctx);

	while (!try_parse_semicolon(ctx))
	{
		std::string param_name = parse_ident(ctx);
		if (param_name == "TDI")
			stmt->tdi = parse_vector(ctx, stmt->length);
		else if (param_name == "TDO")
			stmt->tdo = parse_vector(ctx, stmt->length);
		else if (param_name == "MASK")
			stmt->mask = parse_vector(ctx, stmt->length);
		else if (param_name == "SMASK")
			stmt->smask = parse_vector(ctx, stmt->length);
		else
			throw std::runtime_error("svf: unexpected argument, expected one of TDI, TDO, MASK or SMASK");
	}

	ctx.stmts.push_back(std::move(stmt));
}

svf_file yb::svf_parse(std::istream & s)
{
	parse_context ctx = { s.rdbuf() };
	for (;;)
	{
		std::string command = parse_ident(ctx);
		if (command.empty())
			break;

		command = upper(std::move(command));
		if (command == "ENDDR" || command == "ENDIR")
		{
			std::unique_ptr<svf_endxr> stmt(new svf_endxr(command == "ENDIR"));
			stmt->final_state = parse_tap_state(ctx);
			parse_semicolon(ctx);

			if (!is_final_state(stmt->final_state))
				throw std::runtime_error("ENDxR commands require a *stable* state as an argument.");
			ctx.stmts.push_back(std::move(stmt));
		}
		else if (command == "FREQUENCY")
		{
			std::unique_ptr<svf_frequency> stmt(new svf_frequency());
			if (try_parse_semicolon(ctx))
			{
				stmt->cycles_hz = 0;
			}
			else
			{
				stmt->cycles_hz = parse_real(ctx);
				std::string units = parse_ident(ctx);
				if (upper(units) != "HZ")
					throw std::runtime_error("svf: the FREQUENCY command only accepts HZ as a unit");
				parse_semicolon(ctx);
			}
			ctx.stmts.push_back(std::move(stmt));
		}
		else if (command == "HDR")
		{
			parse_xxr(ctx, ssk_hdr);
		}
		else if (command == "HIR")
		{
			parse_xxr(ctx, ssk_hir);
		}
		else if (command == "SDR")
		{
			parse_xxr(ctx, ssk_sdr);
		}
		else if (command == "SIR")
		{
			parse_xxr(ctx, ssk_sir);
		}
		else if (command == "TDR")
		{
			parse_xxr(ctx, ssk_tdr);
		}
		else if (command == "TIR")
		{
			parse_xxr(ctx, ssk_tir);
		}
		else if (command == "RUNTEST")
		{
			ctx.stmts.push_back(parse_runtest(ctx));
		}
		else if (command == "STATE")
		{
			std::unique_ptr<svf_state_path> stmt(new svf_state_path());
			while (!try_parse_semicolon(ctx))
				stmt->states.push_back(parse_tap_state(ctx));

			if (stmt->states.empty())
				throw std::runtime_error("svf: expected a TAP state");
				
			if (!is_final_state(stmt->states.back()))
				throw std::runtime_error("svf: the last argument of the STATE command must name a stable state");

			ctx.stmts.push_back(std::move(stmt));
		}
		else if (command == "TRST")
		{
			std::unique_ptr<svf_trst> stmt(new svf_trst());
			std::string mode_name = parse_ident(ctx);
			mode_name = upper(std::move(mode_name));
			if (mode_name == "ON")
				stmt->mode = svf_trst::on;
			else if (mode_name == "OFF")
				stmt->mode = svf_trst::off;
			else if (mode_name == "Z")
				stmt->mode = svf_trst::z;
			else if (mode_name == "ABSENT")
				stmt->mode = svf_trst::absent;
			else
				throw std::runtime_error("svf: expected either ON, OFF, Z, or ABSENT as the argument of TRST");
			parse_semicolon(ctx);
			ctx.stmts.push_back(std::move(stmt));
		}
		else
		{
			throw std::runtime_error("unknown command: " + command);
		}
	}
	return std::move(ctx.stmts);
}

static void merge_xxr(svf_xxr & dest, svf_xxr const & source)
{
	// Note that source is not lowered and may therefore have missing
	// tdo, mask and smask. dest should have mask filled and tdo.size() == smask.size().

	assert(dest.kind == source.kind);
	assert(source.tdi.size() == (source.length + 7) / 8);
	assert(source.tdo.empty() || source.tdo.size() == (source.length + 7) / 8);
	assert(source.mask.empty() || !source.tdo.empty());

	size_t vlen = (source.length + 7) / 8;
	if (dest.length != source.length)
	{
		if (source.length != 0 && source.tdi.empty())
			throw std::runtime_error("svf: TDI must be specified whenever the length changes");

		dest = source;
		if (dest.mask.empty())
			dest.mask.resize(vlen, dest.tdo.empty()? 0: 0xff);
		if (dest.tdo.empty())
			dest.tdo.resize(vlen, 0);
		if (dest.smask.empty())
			dest.smask.resize(vlen, 0xff);

		if (vlen)
		{
			size_t rem = source.length % 8;
			uint8_t last_byte = rem? uint8_t(1<<rem) - 1: 0xff;

			dest.tdi.back() &= last_byte;
			dest.tdo.back() &= last_byte;
			dest.mask.back() &= last_byte;
			dest.smask.back() &= last_byte;
		}
	}
	else
	{
		if (!source.tdi.empty())
			dest.tdi = source.tdi;
		dest.tdo = source.tdo;
		if (dest.tdo.empty())
			dest.tdo.resize(vlen, 0);
		if (!source.mask.empty())
			dest.mask = source.mask;
		if (!source.smask.empty())
			dest.smask = source.smask;
	}
}

static void join_bitset(size_t lhs_length, std::vector<uint8_t> & lhs, size_t rhs_length, std::vector<uint8_t> const & rhs)
{
	size_t rem = lhs_length % 8;
	if (rem)
	{
		lhs.reserve((lhs_length + rhs_length + 7) / 8);

		for (size_t i = 0; i < rhs.size(); ++i)
		{
			uint8_t v = rhs[i];
			lhs.back() |= v << rem;
			lhs.push_back(v >> (8 - rem));
		}

		lhs.resize((lhs_length + rhs_length + 7) / 8);
	}
	else
	{
		lhs.insert(lhs.end(), rhs.begin(), rhs.end());
	}
}

static void join_xxr(svf_xxr & lhs, svf_xxr const & rhs)
{
	join_bitset(lhs.length, lhs.tdo, rhs.length, rhs.tdo);
	join_bitset(lhs.length, lhs.tdi, rhs.length, rhs.tdi);
	join_bitset(lhs.length, lhs.mask, rhs.length, rhs.mask);
	join_bitset(lhs.length, lhs.smask, rhs.length, rhs.smask);
	lhs.length += rhs.length;
}

static svf_xxr prepare_shift(svf_xxr const & head, svf_xxr const & body, svf_xxr const & tail)
{
	svf_xxr res = head;
	join_xxr(res, body);
	join_xxr(res, tail);

	if (std::count(res.mask.begin(), res.mask.end(), 0) == res.mask.size())
	{
		res.tdo.clear();
		res.mask.clear();
	}

	res.kind = body.kind;
	return res;
}

static svf_state append_tms_path(size_t & res_length, std::vector<uint8_t> & res, svf_state initial, svf_state final)
{
	static char const * default_paths[4][4] = {
							/* tap_reset   tap_idle   tap_drpause   tap_irpause*/
		/* tap_reset */   { "1",        "0",       "01010",      "011010" },
		/* tap_idle */    { "111",      "0",       "1010",       "11010" },
		/* tap_drpause */ { "11111",    "110",     "111010",     "1111010" },
		/* tap_irpause */ { "11111",    "110",     "111010",     "1111010" },
	};

	size_t pos = res_length % 8;
	uint8_t mask = pos? (1<<pos): 0;

	char const * p = default_paths[initial - 1][final - 1];
	for (; *p; ++p)
	{
		if (!mask)
		{
			res.push_back(0);
			mask = (1<<0);
		}

		if (*p == '1')
			res.back() |= mask;
		mask <<= 1;
		++res_length;
	}

	return final;
}

static svf_state append_tms_path(size_t & res_length, std::vector<uint8_t> & res, svf_state initial, std::vector<svf_state> const & s)
{
	assert(initial >= 1 && initial < 5);
	assert(!s.empty());
	if (s.back() < 1 && s.back() >= 5)
		throw std::runtime_error("The last state in the state path must be stable.");

	if (s.size() == 1)
		return append_tms_path(res_length, res, initial, s[0]);

	static svf_state const edges[16][2] = {
		/*tap_reset,     */ { tap_idle,      tap_reset },
		/*tap_idle,      */ { tap_idle,      tap_drselect },
		/*tap_drpause,   */ { tap_drpause,   tap_drexit2 },
		/*tap_irpause,   */ { tap_irpause,   tap_irexit2 },
		/*tap_drselect,  */ { tap_drcapture, tap_irselect },
		/*tap_drcapture, */ { tap_drshift,   tap_drexit1 },
		/*tap_drshift,   */ { tap_drshift,   tap_drexit1 },
		/*tap_drexit1,   */ { tap_drpause,   tap_drupdate },
		/*tap_drexit2,   */ { tap_drshift,   tap_drupdate },
		/*tap_drupdate,  */ { tap_idle,      tap_drselect },
		/*tap_irselect,  */ { tap_ircapture, tap_reset },
		/*tap_ircapture, */ { tap_irshift,   tap_irexit1 },
		/*tap_irshift,   */ { tap_irshift,   tap_irexit1 },
		/*tap_irexit1,   */ { tap_irpause,   tap_irupdate },
		/*tap_irexit2,   */ { tap_irshift,   tap_irupdate },
		/*tap_irupdate,  */ { tap_idle,      tap_drselect },
	};

	size_t pos = res_length % 8;
	uint8_t mask = pos? (1<<pos): 0;

	for (size_t i = 0; i < s.size(); ++i)
	{
		svf_state next_state = s[i];
		svf_state const (&row)[2] = edges[initial - 1];

		if (!mask)
		{
			res.push_back(0);
			mask = (1<<0);
		}

		if (row[1] == next_state)
			res.back() |= mask;
		else if (row[0] != next_state)
			throw std::runtime_error("invalid state path");

		mask <<= 1;
		++res_length;
		initial = next_state;
	}

	return initial;
}

template <typename Target, typename F>
static svf_state make_tms_path(svf_state initial, Target const & target, F const & f)
{
	std::unique_ptr<svf_tms_path> tms_path(new svf_tms_path());

	// If this is the first transition, we should force a reset.
	if (initial == tap_unknown)
	{
		tms_path->path.push_back(0x1f);
		tms_path->length = 5;
		initial = tap_reset;
	}

	svf_state final = append_tms_path(tms_path->length, tms_path->path, initial, target);
	tms_path->final_state = final;

	std::unique_ptr<svf_statement> new_stmt(tms_path.release());
	f(new_stmt);

	return final;
}

template <typename F>
static void svf_lower_impl(svf_file & doc, F const & f)
{
	svf_state tap_state = tap_unknown;

	svf_state enddr = tap_idle;
	svf_state endir = tap_idle;

	svf_state default_runtest_runstate = tap_idle;
	svf_state default_runtest_endstate = tap_idle;

	svf_xxr hdr(ssk_hdr);
	svf_xxr hir(ssk_hir);
	svf_xxr tdr(ssk_tdr);
	svf_xxr tir(ssk_tir);
	svf_xxr sdr(ssk_sdr);
	svf_xxr sir(ssk_sir);

	for (size_t i = 0; i < doc.size(); ++i)
	{
		std::unique_ptr<svf_statement> & stmt = doc[i];

		switch (stmt->kind)
		{
		case ssk_comment:
		case ssk_frequency:
		case ssk_trst:
			f(stmt);
			break;
		case ssk_endir:
			endir = static_cast<svf_endxr *>(stmt.get())->final_state;
			break;
		case ssk_enddr:
			enddr = static_cast<svf_endxr *>(stmt.get())->final_state;
			break;
		case ssk_hdr:
			merge_xxr(hdr, *static_cast<svf_xxr *>(stmt.get()));
			break;
		case ssk_hir:
			merge_xxr(hir, *static_cast<svf_xxr *>(stmt.get()));
			break;
		case ssk_tdr:
			merge_xxr(tdr, *static_cast<svf_xxr *>(stmt.get()));
			break;
		case ssk_tir:
			merge_xxr(tir, *static_cast<svf_xxr *>(stmt.get()));
			break;
		case ssk_sdr:
			{
				svf_xxr * s = static_cast<svf_xxr *>(stmt.get());
				merge_xxr(sdr, *s);

				if (tap_state != tap_drpause)
					tap_state = make_tms_path(tap_state, tap_drpause, f);

				std::unique_ptr<svf_statement> joined(new svf_xxr(prepare_shift(hdr, sdr, tdr)));
				f(joined);

				if (tap_state != enddr)
					tap_state = make_tms_path(tap_state, enddr, f);
			}
			break;
		case ssk_sir:
			{
				svf_xxr * s = static_cast<svf_xxr *>(stmt.get());
				merge_xxr(sir, *s);

				if (tap_state != tap_irpause)
					tap_state = make_tms_path(tap_state, tap_irpause, f);

				std::unique_ptr<svf_statement> joined(new svf_xxr(prepare_shift(hir, sir, tir)));
				f(joined);

				if (tap_state != endir)
					tap_state = make_tms_path(tap_state, endir, f);
			}
			break;
		case ssk_state:
			tap_state = make_tms_path(tap_state, static_cast<svf_state_path *>(stmt.get())->states, f);
			break;
		case ssk_runtest:
			{
				svf_runtest * s = static_cast<svf_runtest *>(stmt.get());
				if (s->run_state != tap_unknown)
					default_runtest_runstate = s->run_state;

				svf_state run_state = default_runtest_runstate;
				if (tap_state != run_state)
					tap_state = make_tms_path(tap_state, run_state, f);
				
				std::unique_ptr<svf_statement> ns(new svf_runtest(*s));
				f(ns);

				if (s->end_state != tap_unknown)
					default_runtest_endstate = s->end_state;

				svf_state end_state = default_runtest_endstate;
				if (tap_state != end_state)
					tap_state = make_tms_path(tap_state, end_state, f);
			}
			break;
		}
	}
}

svf_file yb::svf_lower(svf_file & doc)
{
	svf_file res;
	svf_lower_impl(doc, [&res](std::unique_ptr<svf_statement> & stmt) {
		res.push_back(std::move(stmt));
	});
	return res;
}

void yb::svf_lower(svf_file & doc, std::function<void(std::unique_ptr<svf_statement> &)> const & visitor)
{
	svf_lower_impl(doc, visitor);
}
