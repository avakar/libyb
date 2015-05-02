#ifndef LIBYB_ASYNC_SIGINT_HPP
#define LIBYB_ASYNC_SIGINT_HPP

#include "task.hpp"

namespace yb {

task<void> wait_for_sigint();

} // namespace yb

#endif // LIBYB_ASYNC_SIGINT_HPP
