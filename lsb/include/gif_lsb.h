#ifndef GIF_LSB_INTERFACE_H
#define GIF_LSB_INTERFACE_H

#include "options.h"

namespace GIFLsb {

bool
gifLsbEncode(const GIFLsb::EncodeOptions& args) noexcept;

bool
gifLsbDecode(const GIFLsb::DecodeOptions& args) noexcept;

}  // namespace GIFLsb

#endif  // GIF_LSB_INTERFACE_H