//consumers: fme / ime
//supplier: db
#include "top.h"
#include "../../common/include/custom_packet.h"
#include <cstring>

Top::Top(sc_module_name name)
	: sc_module(name)
	, frame_memory("frame_memory")
	, db_socket("db_socket")
{
	// register transport handler so initiators can talk to this Top
	db_socket.register_b_transport(this, &Top::b_transport);
}

void Top::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	try {
		auto cmd = trans.get_command();
		uint64_t addr = trans.get_address();
		unsigned char *data = trans.get_data_ptr();
		unsigned int len = trans.get_data_length();

		// decode Rec-style packed address (yy:9, xx:9, size:2, pad:2, plane:2)
		uint32_t yy = static_cast<uint32_t>(addr & 0x1FFULL);
		uint32_t xx = static_cast<uint32_t>((addr >> 9) & 0x1FFULL);
		uint8_t size4x4 = static_cast<uint8_t>((addr >> 18) & 0x3ULL);
		PaddingMode pad = static_cast<PaddingMode>((addr >> 20) & 0x3ULL);
		RecPlane plane = static_cast<RecPlane>((addr >> 22) & 0x3ULL);

		if (cmd == tlm::TLM_READ_COMMAND) {
			// If caller expects an (N+2)x(N+2) window, use getRefBlock
			uint32_t N = recSizeToPixels(size4x4);
			uint32_t ext = N + 2u;
			size_t expect_bytes = static_cast<size_t>(ext) * static_cast<size_t>(ext);
			if (size4x4 != 0 || len == expect_bytes) {
				RefBlock out;
				if (!frame_memory.getRefBlock(plane, xx, yy, size4x4, pad, out)) {
					trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
					return;
				}
				size_t copy = std::min<size_t>(len, out.data.size());
				if (data && copy) std::memcpy(data, out.data.data(), copy);
				trans.set_response_status(tlm::TLM_OK_RESPONSE);
				return;
			}

			// Otherwise treat as a row read
			if (!data) { trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
			if (!frame_memory.read_row(plane, xx, yy, data, len)) {
				trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
				return;
			}
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
			return;
		}

		if (cmd == tlm::TLM_WRITE_COMMAND) {
			if (!data) { trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }

			// Attempt to interpret payload as a packed CustomPacket from DB
			CustomPacket cp = unpackCustomPacket(data, len);
			if (!cp.data.empty()) {
				// map block coords to pixel coords
				uint32_t px = static_cast<uint32_t>(cp.x) * 4u;
				uint32_t py = static_cast<uint32_t>(cp.y) * 4u;
				RecPlane p = (cp.sel <= 2) ? static_cast<RecPlane>(cp.sel) : RecPlane::Y;

				// if we have at least 16 bytes, write first 4x4 block
				if (cp.data.size() >= 16) {
					for (uint32_t r = 0; r < 4; ++r) {
						const uint8_t *row_ptr = cp.data.data() + static_cast<size_t>(r) * 4;
						frame_memory.write_row(p, px, py + r, row_ptr, 4, 0);
					}
				}
				// if we have a second 4x4 (q block), write it to the right
				if (cp.data.size() >= 32) {
					for (uint32_t r = 0; r < 4; ++r) {
						const uint8_t *row_ptr = cp.data.data() + 16 + static_cast<size_t>(r) * 4;
						frame_memory.write_row(p, px + 4u, py + r, row_ptr, 4, 0);
					}
				}
				trans.set_response_status(tlm::TLM_OK_RESPONSE);
				return;
			}

			// If payload matches an (N+2)x(N+2) window, treat as pushRefBlock
			uint32_t N = recSizeToPixels(size4x4);
			uint32_t ext = N + 2u;
			size_t expect_bytes = static_cast<size_t>(ext) * static_cast<size_t>(ext);
			if (len >= expect_bytes) {
				RefBlock in;
				in.width = ext; in.height = ext; in.stride = ext;
				in.data.assign(data, data + expect_bytes);
				frame_memory.pushRefBlock(plane, xx, yy, size4x4, in, 0);
				trans.set_response_status(tlm::TLM_OK_RESPONSE);
				return;
			}

			// Otherwise treat as a row write
			frame_memory.write_row(plane, xx, yy, data, len, 0);
			trans.set_response_status(tlm::TLM_OK_RESPONSE);
			return;
		}

		trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
	} catch (...) {
		trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
	}
}