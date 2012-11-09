#ifndef LIBYB_UTILS_UTF_HPP
#define LIBYB_UTILS_UTF_HPP

#include "../vector_ref.hpp"

namespace yb {

std::string utf16le_to_utf8(buffer_ref const & source);

} // namespace yb

#endif // LIBYB_UTILS_UTF_HPP
