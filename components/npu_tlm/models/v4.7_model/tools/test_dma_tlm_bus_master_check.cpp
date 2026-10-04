// Checks that SauriaDmaTlm (control/sauria_dma_tlm.h, a proof of concept, not the control/sauria_dma.h used by
// npu_top.h) issues real TLM-2.0 transactions to an external memory instead of using a backdoor pointer.
//
// Scenario: write a pattern into SRAM bank 2 (backdoor, stimulus only), push it with SauriaDmaTlm::start_write()
// to MockDramTlm, check MockDramTlm's internal memory directly, then read it back with start_read() into
// another SRAM bank and check the round trip.
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_target_socket.h>
#include <iostream>
#include <vector>
#include <cstring>

#include "sauria_types.h"
#include "sram/sram_top.h"
#include "control/sauria_dma_tlm.h"

using namespace sauria;

// Minimal mock DRAM with a real TLM-2.0 target socket (same pattern as adc_tlm/clint_tlm: zero-wait
// b_transport(), direct memcpy).
SC_MODULE(MockDramTlm)
{
    tlm_utils::simple_target_socket<MockDramTlm> socket;
    std::vector<uint8_t> mem;
    int transactions_seen = 0;

    SC_CTOR(MockDramTlm) : socket("socket")
    {
        mem.resize(1024 * 1024, 0);
        socket.register_b_transport(this, &MockDramTlm::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_time &delay)
    {
        transactions_seen++;
        uint64_t addr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned len = trans.get_data_length();

        if (addr + len > mem.size())
        {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND)
        {
            std::memcpy(&mem[addr], ptr, len);
        }
        else if (trans.get_command() == tlm::TLM_READ_COMMAND)
        {
            std::memcpy(ptr, &mem[addr], len);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

typedef SauriaDmaTlm<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaTlmT;
typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;

SC_MODULE(TbDmaTlmBusMasterCheck)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn;
    sc_signal<bool> sram_deepsleep, sram_powergate;
    sc_signal<sc_bv<3>> sram_select;
    sc_signal<uint32_t> host_addr;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata, host_rdata;
    sc_signal<host_mask_t> host_wmask;
    // Dummy signals only to bind every Sram port (SystemC requires all ports bound); this test uses the backdoor
    // read/write_bank_data only.
    sc_signal<uint32_t> d_addr_a1, d_addr_b1, d_addr_a2, d_addr_b2, d_addr_a3, d_addr_b3;
    sc_signal<bool> d_rden_a1, d_rden_b1, d_rden_a2, d_rden_b2, d_wren_a3, d_rden_a3, d_wren_b3, d_rden_b3;
    sc_signal<act_vector_t<32, int8_t>> d_data_a1, d_data_b1;
    sc_signal<wei_vector_t<32, int8_t>> d_data_a2, d_data_b2;
    sc_signal<psum_vector_t<32, int32_t>> d_wdata_a3, d_rdata_a3, d_wdata_b3, d_rdata_b3;
    sc_signal<sramc_mask_t<32>> d_wmask_a3, d_wmask_b3;

    DmaTlmT *dma;
    SramT *sram;
    MockDramTlm *dram;
    int errors = 0;

    SC_CTOR(TbDmaTlmBusMasterCheck)
    {
        dma = new DmaTlmT("dma");
        sram = new SramT("sram");
        dram = new MockDramTlm("dram");

        dma->i_clk(i_clk);
        dma->i_rstn(rstn);
        dma->dram_socket.bind(dram->socket);
        dma->set_sram(sram);

        sram->i_clk(i_clk);
        sram->i_rstn(rstn);
        sram->i_deepsleep(sram_deepsleep);
        sram->i_powergate(sram_powergate);
        sram->i_select(sram_select);
        sram->i_host_addr(host_addr);
        sram->i_host_wren(host_wren);
        sram->i_host_rden(host_rden);
        sram->i_host_wdata(host_wdata);
        sram->i_host_wmask(host_wmask);
        sram->o_host_rdata(host_rdata);
        sram->i_srama_addr_a(d_addr_a1); sram->i_srama_rden_a(d_rden_a1); sram->o_srama_data_a(d_data_a1);
        sram->i_srama_addr_b(d_addr_b1); sram->i_srama_rden_b(d_rden_b1); sram->o_srama_data_b(d_data_b1);
        sram->i_sramb_addr_a(d_addr_a2); sram->i_sramb_rden_a(d_rden_a2); sram->o_sramb_data_a(d_data_a2);
        sram->i_sramb_addr_b(d_addr_b2); sram->i_sramb_rden_b(d_rden_b2); sram->o_sramb_data_b(d_data_b2);
        sram->i_sramc_wdata_a(d_wdata_a3); sram->i_sramc_addr_a(d_addr_a3); sram->i_sramc_wren_a(d_wren_a3);
        sram->i_sramc_rden_a(d_rden_a3); sram->i_sramc_wmask_a(d_wmask_a3); sram->o_sramc_rdata_a(d_rdata_a3);
        sram->i_sramc_wdata_b(d_wdata_b3); sram->i_sramc_addr_b(d_addr_b3); sram->i_sramc_wren_b(d_wren_b3);
        sram->i_sramc_rden_b(d_rden_b3); sram->i_sramc_wmask_b(d_wmask_b3); sram->o_sramc_rdata_b(d_rdata_b3);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    void run()
    {
        rstn.write(false);
        host_wren.write(false);
        host_rden.write(false);
        sram_deepsleep.write(false);
        sram_powergate.write(false);
        sram_select.write(0);
        wait(5);
        rstn.write(true);
        wait(5);

        std::cout << "==================================================" << std::endl;
        std::cout << " DMA TLM BUS-MASTER CHECK (proof of concept, not the sauria_dma.h used by npu_top.h)" << std::endl;
        std::cout << "==================================================" << std::endl;

        const int N = 32;
        std::vector<uint8_t> pattern(N);
        for (int i = 0; i < N; i++) pattern[i] = static_cast<uint8_t>(0xA0 + i);

        // Seed SRAM bank 2 (ifmap) through the backdoor -- source data only, not what is being tested.
        sram->write_bank_data(2, 0, pattern.data(), N);

        std::cout << "\n[DMA] start_write(dram_addr=0, bank=2, size=" << N << ") -- day qua TLM toi MockDramTlm..." << std::endl;
        dma->start_write(/*dram_addr=*/0, /*bank_id=*/2, /*bank_offset=*/0, /*size_bytes=*/N);

        int timeout = 0;
        while (dma->is_write_active() && timeout < 200) { wait(); timeout++; }
        wait(2);

        check(!dma->is_write_active(), "DMA write completes before timeout");
        check(dram->transactions_seen >= 1, "MockDramTlm received at least one real TLM transaction (not a backdoor)");

        bool write_ok = true;
        for (int i = 0; i < N; i++) if (dram->mem[i] != pattern[i]) write_ok = false;
        check(write_ok, "Data in MockDramTlm matches the written pattern (read directly from the target's memory, not through the DMA)");

        std::cout << "\n[DMA] start_read(ch=0, dram_addr=0, bank=0, size=" << N << ") -- doc nguoc qua TLM tu MockDramTlm..." << std::endl;
        int tx_before_read = dram->transactions_seen;
        dma->start_read(/*ch_id=*/0, /*dram_addr=*/0, /*bank_id=*/0, /*bank_offset=*/0, /*size_bytes=*/N);

        timeout = 0;
        while (dma->is_any_read_active() && timeout < 200) { wait(); timeout++; }
        wait(2);

        check(!dma->is_any_read_active(), "DMA read completes before timeout");
        check(dram->transactions_seen > tx_before_read, "A new TLM transaction for the read (real round trip, no cache/backdoor)");

        std::vector<uint8_t> readback(N, 0);
        sram->read_bank_data(0, 0, readback.data(), N);
        bool read_ok = true;
        for (int i = 0; i < N; i++) if (readback[i] != pattern[i]) read_ok = false;
        check(read_ok, "SRAM bank 0 after the DMA read back through TLM matches the original pattern (SRAM->TLM->SRAM round trip)");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] DMA TLM BUS-MASTER CHECK." << std::endl;
        else std::cout << "  [FAIL] DMA TLM BUS-MASTER CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbDmaTlmBusMasterCheck tb("TbDmaTlmBusMasterCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
