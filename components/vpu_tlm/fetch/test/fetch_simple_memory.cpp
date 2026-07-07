#include "fetch_simple_memory.h"
#include <iostream>

FetchSimpleMemory::FetchSimpleMemory(sc_core::sc_module_name name)
    : sc_core::sc_module(name), socket("socket")
{
    socket.register_b_transport(this, &FetchSimpleMemory::b_transport);
}

void FetchSimpleMemory::load_data(uint64_t addr, const std::vector<uint8_t>& data) {
    uint64_t a = addr;
    for (size_t i = 0; i < data.size(); ++i) mem_[a + i] = data[i];
}

void FetchSimpleMemory::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
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
        // Debug
        std::cerr << "FetchSimpleMemory: READ addr=0x" << std::hex << addr << std::dec << " len=" << len << " ->";
        for (unsigned int i = 0; i < std::min<unsigned int>(len, 8); ++i) std::cerr << " " << static_cast<int>(ptr[i]);
        std::cerr << std::endl;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    if (cmd == tlm::TLM_WRITE_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) mem_[addr + i] = ptr[i];
        // Debug
        std::cerr << "FetchSimpleMemory: WRITE addr=0x" << std::hex << addr << std::dec << " len=" << len << " <-";
        for (unsigned int i = 0; i < std::min<unsigned int>(len, 8); ++i) std::cerr << " " << static_cast<int>(ptr[i]);
        std::cerr << std::endl;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }

    trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
}
