#include "memory_map.h"
#include <iostream>

namespace h264::mem {

MemoryMap::MemoryMap(sc_core::sc_module_name name) 
    : sc_core::sc_module(name) {
    
    internal_mem_.resize(INT_MEM_SIZE, 0);
    external_mem_.resize(EXT_MEM_SIZE, 0);
    
    socket.register_b_transport(this, &MemoryMap::b_transport);
}

void MemoryMap::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tlm::tlm_command cmd = tx.get_command();
    std::uint64_t addr   = tx.get_address();
    unsigned char* ptr   = tx.get_data_ptr();
    unsigned int len     = tx.get_data_length();
    unsigned char* byte_en = tx.get_byte_enable_ptr();
    unsigned int be_len    = tx.get_byte_enable_length();

    bool is_internal = (addr >= INT_MEM_BASE && (addr + len) <= (INT_MEM_BASE + INT_MEM_SIZE));
    bool is_external = (addr >= EXT_MEM_BASE && (addr + len) <= (EXT_MEM_BASE + EXT_MEM_SIZE));

    if (!is_internal && !is_external) {
        std::cout << "Truy cập ngoài phân vùng cho phép: 0x" << std::hex << addr << std::dec << "\n";
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    std::uint8_t* target_mem = is_internal ? internal_mem_.data() : external_mem_.data();
    std::uint32_t offset = is_internal ? (addr - INT_MEM_BASE) : (addr - EXT_MEM_BASE);

    if (cmd == tlm::TLM_WRITE_COMMAND) {
        // Thực thi ghi bộ nhớ, tuân thủ chặt chẽ WSTRB từ AXI
        for (unsigned int i = 0; i < len; ++i) {
            if (!byte_en || byte_en[i % be_len] == TLM_BYTE_ENABLED) {
                target_mem[offset + i] = ptr[i];
            }
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    } 
    else if (cmd == tlm::TLM_READ_COMMAND) {
        for (unsigned int i = 0; i < len; ++i) {
            ptr[i] = target_mem[offset + i];
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    delay += sc_core::sc_time(10, sc_core::SC_NS);
}

} // namespace h264::mem