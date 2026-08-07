#ifndef ISP_TLM_AXI_ISP_AXI_LITE_ADAPTER_H
#define ISP_TLM_AXI_ISP_AXI_LITE_ADAPTER_H

#include "registers/isp_register_bank.h"

#include <systemc>

#include <cstdint>

namespace isp_tlm::axi {

class IspAxiLiteAdapter : public sc_core::sc_module {
public:
    static constexpr std::uint8_t kOkay = 0u;
    static constexpr std::uint8_t kSlvErr = 2u;
    static constexpr std::uint8_t kDecErr = 3u;

    sc_core::sc_in<bool> aclk {"aclk"};
    sc_core::sc_in<bool> aresetn {"aresetn"};

    sc_core::sc_in<sc_dt::sc_uint<32>> awaddr {"awaddr"};
    sc_core::sc_in<sc_dt::sc_uint<3>> awprot {"awprot"};
    sc_core::sc_in<bool> awvalid {"awvalid"};
    sc_core::sc_out<bool> awready {"awready"};

    sc_core::sc_in<sc_dt::sc_uint<32>> wdata {"wdata"};
    sc_core::sc_in<sc_dt::sc_uint<4>> wstrb {"wstrb"};
    sc_core::sc_in<bool> wvalid {"wvalid"};
    sc_core::sc_out<bool> wready {"wready"};

    sc_core::sc_out<sc_dt::sc_uint<2>> bresp {"bresp"};
    sc_core::sc_out<bool> bvalid {"bvalid"};
    sc_core::sc_in<bool> bready {"bready"};

    sc_core::sc_in<sc_dt::sc_uint<32>> araddr {"araddr"};
    sc_core::sc_in<sc_dt::sc_uint<3>> arprot {"arprot"};
    sc_core::sc_in<bool> arvalid {"arvalid"};
    sc_core::sc_out<bool> arready {"arready"};

    sc_core::sc_out<sc_dt::sc_uint<32>> rdata {"rdata"};
    sc_core::sc_out<sc_dt::sc_uint<2>> rresp {"rresp"};
    sc_core::sc_out<bool> rvalid {"rvalid"};
    sc_core::sc_in<bool> rready {"rready"};

    SC_HAS_PROCESS(IspAxiLiteAdapter);
    IspAxiLiteAdapter(sc_core::sc_module_name name,
                      registers::IspRegisterBank& register_bank)
        : sc_core::sc_module(name)
        , register_bank_(register_bank)
    {
        awready.initialize(false);
        wready.initialize(false);
        bresp.initialize(kOkay);
        bvalid.initialize(false);
        arready.initialize(false);
        rdata.initialize(0u);
        rresp.initialize(kOkay);
        rvalid.initialize(false);

        SC_METHOD(tick);
        sensitive << aclk.pos();
        dont_initialize();
    }

private:
    registers::IspRegisterBank& register_bank_;

    bool aw_captured_ = false;
    std::uint32_t captured_awaddr_ = 0u;
    bool w_captured_ = false;
    std::uint32_t captured_wdata_ = 0u;
    std::uint8_t captured_wstrb_ = 0u;
    bool bvalid_state_ = false;
    std::uint8_t bresp_state_ = kOkay;

    bool rvalid_state_ = false;
    std::uint32_t rdata_state_ = 0u;
    std::uint8_t rresp_state_ = kOkay;

    static std::uint8_t response_for(registers::AccessResult result)
    {
        switch (result) {
        case registers::AccessResult::Ok:
            return kOkay;
        case registers::AccessResult::Misaligned:
        case registers::AccessResult::OutOfRange:
        case registers::AccessResult::Unmapped:
            return kDecErr;
        case registers::AccessResult::ReadOnly:
        case registers::AccessResult::InvalidByteStrobe:
        case registers::AccessResult::InvalidValue:
            return kSlvErr;
        }
        return kSlvErr;
    }

    void drive_outputs()
    {
        bresp.write(bresp_state_);
        bvalid.write(bvalid_state_);
        rdata.write(rdata_state_);
        rresp.write(rresp_state_);
        rvalid.write(rvalid_state_);

        const bool write_slot_available = !bvalid_state_;
        awready.write(write_slot_available && !aw_captured_);
        wready.write(write_slot_available && !w_captured_);
        arready.write(!rvalid_state_);
    }

    void reset_state()
    {
        aw_captured_ = false;
        captured_awaddr_ = 0u;
        w_captured_ = false;
        captured_wdata_ = 0u;
        captured_wstrb_ = 0u;
        bvalid_state_ = false;
        bresp_state_ = kOkay;
        rvalid_state_ = false;
        rdata_state_ = 0u;
        rresp_state_ = kOkay;

        awready.write(false);
        wready.write(false);
        bresp.write(kOkay);
        bvalid.write(false);
        arready.write(false);
        rdata.write(0u);
        rresp.write(kOkay);
        rvalid.write(false);
        register_bank_.reset();
    }

    void tick()
    {
        if (!aresetn.read()) {
            reset_state();
            return;
        }

        const bool accept_aw = awready.read() && awvalid.read();
        const bool accept_w = wready.read() && wvalid.read();
        const bool accept_b = bvalid_state_ && bready.read();
        const bool accept_ar = arready.read() && arvalid.read();
        const bool accept_r = rvalid_state_ && rready.read();

        if (accept_b) {
            bvalid_state_ = false;
        }
        if (accept_aw) {
            captured_awaddr_ = awaddr.read().to_uint();
            aw_captured_ = true;
        }
        if (accept_w) {
            captured_wdata_ = wdata.read().to_uint();
            captured_wstrb_ =
                static_cast<std::uint8_t>(wstrb.read().to_uint());
            w_captured_ = true;
        }

        if (!bvalid_state_ && aw_captured_ && w_captured_) {
            const registers::AccessResult result = register_bank_.write(
                captured_awaddr_, captured_wdata_, captured_wstrb_);
            bresp_state_ = response_for(result);
            bvalid_state_ = true;
            aw_captured_ = false;
            w_captured_ = false;
        }

        if (accept_r) {
            rvalid_state_ = false;
        }
        if (accept_ar) {
            std::uint32_t value = 0u;
            const registers::AccessResult result =
                register_bank_.read(araddr.read().to_uint(), value);
            rdata_state_ = value;
            rresp_state_ = response_for(result);
            rvalid_state_ = true;
        }

        drive_outputs();
    }
};

}  // namespace isp_tlm::axi

#endif  // ISP_TLM_AXI_ISP_AXI_LITE_ADAPTER_H
