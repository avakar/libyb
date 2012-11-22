#ifndef LIBYB_UTILS_IHEX_FILE_HPP
#define LIBYB_UTILS_IHEX_FILE_HPP

#include "../vector_ref.hpp"
#include "sparse_buffer.hpp"
#include <istream>
#include <exception>

namespace yb {

class ihex_parse_error
	: public std::exception
{
public:
	enum kind_t
	{
		k_missing_leading_colon,
		k_mismatched_record_length,
		k_unknown_record_type,
		k_overlapping_regions,
		k_unexpected_character,
	};

	ihex_parse_error(kind_t kind, int line);

	const char * what() const throw();
	kind_t kind() const;
	int line() const;

private:
	kind_t m_kind;
	int m_line;
};

sparse_buffer parse_ihex(string_ref const & data);
sparse_buffer parse_ihex(std::istream & data);

} // namespace yb

#endif // LIBYB_UTILS_IHEX_FILE_HPP
