#ifndef LIBYB_UTILS_LEXCAST_HPP
#define LIBYB_UTILS_LEXCAST_HPP

namespace yb {

template <typename T, typename U>
T lexical_cast(U const & u);

} // namespace yb

#include "detail/lexcast_impl.hpp"

#endif // LIBYB_UTILS_LEXCAST_HPP
