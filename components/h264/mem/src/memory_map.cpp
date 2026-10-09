#include "memory_map.h"
#include <iostream>

namespace h264::mem {
MemoryMap::MemoryMap(sc_core::sc_module_name name) 
    : sc_core::sc_module(name), internal_mem_(INT_MEM_SIZE), external_mem_(EXT_MEM_SIZE) {
    socket.register_b_transport(this, &MemoryMap::b_transport);
}

void MemoryMap::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    delay += sc_core::sc_time(1, sc_core::SC_NS);
    auto* data = tx.get_data_ptr();
    const auto addr = tx.get_address();
    const auto len = tx.get_data_length();
    
    if (!data) { 
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return; 
    }

    bool success = false;

    // Address Decoding
    if (addr >= INT_MEM_BASE && (addr + len) <= (INT_MEM_BASE + INT_MEM_SIZE)) {
        // Giao dịch thuộc Internal Memory (SRAM)
        std::uint32_t offset = addr - INT_MEM_BASE;

        if (tx.get_command() == tlm::TLM_WRITE_COMMAND) 
            success = internal_mem_.write(offset, data, len);
        else if (tx.get_command() == tlm::TLM_READ_COMMAND) 
            success = internal_mem_.read(offset, data, len);
        
    } else if (addr >= EXT_MEM_BASE && (addr + len) <= (EXT_MEM_BASE + EXT_MEM_SIZE)) {
        // Giao dịch thuộc External Memory (DRAM)
        std::uint32_t offset = addr - EXT_MEM_BASE;
        
        if (tx.get_command() == tlm::TLM_WRITE_COMMAND) 
            success = external_mem_.write(offset, data, len);
        else if (tx.get_command() == tlm::TLM_READ_COMMAND) 
            success = external_mem_.read(offset, data, len);
        
    } else {
        // Địa chỉ nằm ngoài dải quy định
        std::cout << "Truy cập địa chỉ ngoài vùng nhớ: 0x" << std::hex << addr << std::dec << "\n";
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (success) tx.set_response_status(tlm::TLM_OK_RESPONSE);
    else tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
}
} // namespace h264::mem