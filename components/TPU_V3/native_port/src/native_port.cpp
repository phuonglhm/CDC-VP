// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sram/native_port.h"

namespace cdc::components::tpu_v3::sram {

const char* to_string(neo_requester requester) noexcept
{
    switch (requester) {
    case neo_requester::cpu:
        return "cpu";
    case neo_requester::dma:
        return "dma";
    case neo_requester::sa:
        return "sa";
    case neo_requester::transform:
        return "transform";
    case neo_requester::external_inbound:
        return "external_inbound";
    }
    return "unknown";
}

const char* to_string(neo_command command) noexcept
{
    switch (command) {
    case neo_command::read:
        return "read";
    case neo_command::write:
        return "write";
    }
    return "unknown";
}

const char* to_string(neo_status status) noexcept
{
    switch (status) {
    case neo_status::ok:
        return "ok";
    case neo_status::decode_error:
        return "decode_error";
    case neo_status::capacity_error:
        return "capacity_error";
    case neo_status::size_error:
        return "size_error";
    case neo_status::aborted:
        return "aborted";
    }
    return "unknown";
}

} // namespace cdc::components::tpu_v3::sram
