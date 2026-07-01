#include "simple_memory.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

SimpleMemory::SimpleMemory(sc_core::sc_module_name name)
    : sc_core::sc_module(name), socket("socket") {
    socket.register_b_transport(this, &SimpleMemory::b_transport);
    // Auto-load CABAC tables from prebuilt binary files in tables/.
    if (!load_cabac_tables()) {
        std::cerr << "SimpleMemory: warning - no CABAC .bin tables found\n";
    }
}

void SimpleMemory::load_data(uint64_t addr, const std::vector<uint8_t>& data) {
    uint64_t a = addr;
    for (size_t i = 0; i < data.size(); ++i) {
        mem_[a + i] = data[i];
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

    bool loaded_any = false;

    struct BinMap { const char* file; uint64_t base; };
    BinMap bins[] = {
        {"tables/cabac_ctx_islice.bin", CABAC_CTX_BASE_I},
        {"tables/cabac_ctx_init2.bin", CABAC_CTX_BASE_INIT2},
        {"tables/cabac_ctx_init1.bin", CABAC_CTX_BASE_INIT1},
    };

    for (const auto &bm : bins) {
        std::ifstream bin(bm.file, std::ios::binary);
        if (!bin) continue;
        bin.seekg(0, std::ios::end);
        std::streampos n = bin.tellg();
        bin.seekg(0);
        std::vector<uint8_t> data(static_cast<size_t>(n));
        if (n > 0) bin.read(reinterpret_cast<char*>(data.data()), n);
        load_data(bm.base, data);
        std::cerr << "SimpleMemory: loaded " << bm.file << " (" << data.size() << " bytes) at 0x" << std::hex << bm.base << std::dec << "\n";
        loaded_any = true;
    }

    return loaded_any;
}
