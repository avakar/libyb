#ifndef LIBYB_SHUPITO_ESCAPE_SEQUENCE_HPP
#define LIBYB_SHUPITO_ESCAPE_SEQUENCE_HPP

#include "../async/stream.hpp"

namespace yb {

task<void> send_generic_escape_seq(stream & sp, int cycle_length_ms, uint8_t const * escseq, size_t escseq_len, uint8_t const * respseq, size_t respseq_len);
task<void> send_avr232boot_escape_seq(stream & sp, int cycle_length_ms);

} // namespace yb

#endif // LIBYB_SHUPITO_ESCAPE_SEQUENCE_HPP
