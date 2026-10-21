#pragma once
#include "fu3/io_pad/io_pad.h"

// Compatibility for clients of the original combined component.
namespace fu3::gpio {
using fu3::io_pad::IoPad;
using fu3::io_pad::IoSel;
using fu3::io_pad::Pin;
using fu3::io_pad::pin_info;
} // namespace fu3::gpio
