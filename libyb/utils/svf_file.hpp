#ifndef LIBYB_UTILS_SVF_PARSER_HPP
#define LIBYB_UTILS_SVF_PARSER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <cassert>
#include <stdint.h>

namespace yb {

enum svf_state
{
	tap_unknown,

	tap_reset,
	tap_idle,
	tap_drpause,
	tap_irpause,

	tap_drselect,
	tap_drcapture,
	tap_drshift,
	tap_drexit1,
	tap_drexit2,
	tap_drupdate,
	tap_irselect,
	tap_ircapture,
	tap_irshift,
	tap_irexit1,
	tap_irexit2,
	tap_irupdate
};

enum svf_statement_kind
{
	ssk_comment,
	ssk_endir,
	ssk_enddr,
	ssk_frequency,
	ssk_hdr,
	ssk_hir,
	ssk_sdr,
	ssk_sir,
	ssk_tdr,
	ssk_tir,
	ssk_state,
	ssk_trst,
	ssk_runtest,
	ssk_tms_path,
};

struct svf_statement
{
public:
	virtual ~svf_statement() {}
	svf_statement_kind kind;

protected:
	svf_statement(svf_statement_kind kind)
		: kind(kind)
	{
	}
};

struct svf_comment
	: svf_statement
{
	enum comment_kind_t { ck_unknown, ck_bang, ck_slashslash };
	svf_comment()
		: svf_statement(ssk_comment), comment_kind(ck_unknown)
	{
	}

	std::string text;
	comment_kind_t comment_kind;
};

struct svf_endxr
	: svf_statement
{
	explicit svf_endxr(bool ir): svf_statement(ir? ssk_endir: ssk_enddr) {}
	svf_state final_state;
};

struct svf_frequency
	: svf_statement
{
	svf_frequency(): svf_statement(ssk_frequency) {}
	double cycles_hz;
};

struct svf_xxr
	: svf_statement
{
	explicit svf_xxr(svf_statement_kind kind)
		: svf_statement(kind), length(0)
	{
	}

	size_t length;
	std::vector<uint8_t> tdi;
	std::vector<uint8_t> tdo;
	std::vector<uint8_t> mask;
	std::vector<uint8_t> smask;
};

struct svf_state_path
	: svf_statement
{
	svf_state_path(): svf_statement(ssk_state) {}
	std::vector<svf_state> states;
};

struct svf_trst
	: svf_statement
{
	svf_trst(): svf_statement(ssk_trst) {}
	enum mode_t { on, off, z, absent } mode;
};

struct svf_runtest
	: svf_statement
{
	svf_state run_state;
	uint32_t run_count;
	enum { nock, tck, sck } run_clk;
	double min_time;
	double max_time;
	svf_state end_state;

	svf_runtest()
		: svf_statement(ssk_runtest), run_state(), run_count(0), run_clk(nock),
		min_time(0), max_time(std::numeric_limits<double>::infinity()), end_state()
	{
	}
};

struct svf_tms_path
	: svf_statement
{
	size_t length;
	std::vector<uint8_t> path;
	svf_state final_state;

	svf_tms_path()
		: svf_statement(ssk_tms_path), length(0), final_state(tap_unknown)
	{
	}
};

typedef std::vector<std::unique_ptr<svf_statement> > svf_file;

svf_file svf_parse(std::istream & s);
svf_file svf_lower(svf_file & doc);
void svf_lower(svf_file & doc, std::function<void(std::unique_ptr<svf_statement> &)> const & visitor);

template <typename Visitor>
void svf_visit(svf_statement const * stmt, Visitor & visitor)
{
	switch (stmt->kind)
	{
	case ssk_comment:
		visitor(*static_cast<svf_comment const *>(stmt));
		break;
	case ssk_enddr:
	case ssk_endir:
		visitor(*static_cast<svf_endxr const *>(stmt));
		break;
	case ssk_frequency:
		visitor(*static_cast<svf_frequency const *>(stmt));
		break;
	case ssk_hdr:
	case ssk_hir:
	case ssk_sdr:
	case ssk_sir:
	case ssk_tdr:
	case ssk_tir:
		visitor(*static_cast<svf_xxr const *>(stmt));
		break;
	case ssk_runtest:
		visitor(*static_cast<svf_runtest const *>(stmt));
		break;
	case ssk_state:
		visitor(*static_cast<svf_state_path const *>(stmt));
		break;
	case ssk_tms_path:
		visitor(*static_cast<svf_tms_path const *>(stmt));
		break;
	case ssk_trst:
		visitor(*static_cast<svf_trst const *>(stmt));
		break;
	default:
		assert(0);
	}
}

template <typename Visitor>
void svf_visit(svf_file const & doc, Visitor & visitor)
{
	for (size_t i = 0; i < doc.size(); ++i)
		svf_visit(doc[i].get(), visitor);
}

} // namespace yb

#endif // LIBYB_UTILS_SVF_PARSER_HPP
