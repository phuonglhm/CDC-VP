#include "simple_memory.h"

// CABAC tables are header-only and live in the cabac include; implement
// loading here so the shared SimpleMemory can be used by the CABAC block.
#include "../cabac/include/cabac_tables.h"

SimpleMemory::SimpleMemory(sc_core::sc_module_name name)
    : sc_core::sc_module(name), socket("socket") {
    socket.register_b_transport(this, &SimpleMemory::b_transport);
    // Load CABAC tables by default so CABAC block sees initialized contexts
    // when using the shared SimpleMemory implementation.
    load_cabac_tables();
}

void SimpleMemory::load_data(uint64_t addr, const std::vector<uint8_t>& data) {
    uint64_t a = addr;
    for (size_t i = 0; i < data.size(); ++i) {
        mem_[a + i] = data[i];
    }
}

std::vector<uint8_t> SimpleMemory::read_region(uint64_t addr, size_t len) const {
    std::vector<uint8_t> out;
    out.resize(len);
    for (size_t i = 0; i < len; ++i) {
        auto it = mem_.find(addr + i);
        out[i] = (it != mem_.end()) ? it->second : 0;
    }
    return out;
}

void SimpleMemory::write_region(uint64_t addr, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        mem_[addr + i] = data[i];
    }
}

void SimpleMemory::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    tlm::tlm_command cmd = trans.get_command();
    uint64_t addr = trans.get_address();
    unsigned char* ptr = trans.get_data_ptr();
    unsigned int len = trans.get_data_length();

    if (!ptr) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_READ_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) {
            auto it = mem_.find(addr + i);
            ptr[i] = (it != mem_.end()) ? it->second : 0;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_WRITE_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) {
            mem_[addr + i] = ptr[i];
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
}

bool SimpleMemory::load_cabac_tables(const std::vector<std::string>& /*candidates*/) {
    constexpr uint64_t CABAC_CTX_BASE_I = 0x10000000ULL;
    constexpr uint64_t CABAC_CTX_BASE_INIT2 = 0x10000100ULL;
    constexpr uint64_t CABAC_CTX_BASE_INIT1 = 0x10000200ULL;

    // Load embedded CABAC tables (header-provided arrays)
    load_data(CABAC_CTX_BASE_I, std::vector<uint8_t>(cabac_ctx_islice, cabac_ctx_islice + sizeof(cabac_ctx_islice)));
    load_data(CABAC_CTX_BASE_INIT2, std::vector<uint8_t>(cabac_ctx_init2, cabac_ctx_init2 + sizeof(cabac_ctx_init2)));
    load_data(CABAC_CTX_BASE_INIT1, std::vector<uint8_t>(cabac_ctx_init1, cabac_ctx_init1 + sizeof(cabac_ctx_init1)));
    return true;
}
