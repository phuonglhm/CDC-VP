#include <h264/platform/host_driver.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
namespace h264 {
uint32_t HostDriver::read(uint32_t address) {
    unsigned char data[4]{};
    tlm::tlm_generic_payload tx;
    tx.set_command(tlm::TLM_READ_COMMAND); tx.set_address(uint64_t(csr_base_)+address);
    tx.set_data_ptr(data); tx.set_data_length(4); tx.set_streaming_width(4);
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    registers->b_transport(tx,delay); consume_delay(delay);
    if(tx.is_response_error()) throw std::runtime_error("register read failed");
    return load_le(data);
}
tlm::tlm_response_status HostDriver::write(uint32_t address,uint32_t value,unsigned mask) {
    unsigned char data[4],be[4]; store_le(data,value);
    for(unsigned i=0;i<4;++i) be[i]=(mask&(1u<<i)) ? 0xff : 0;
    tlm::tlm_generic_payload tx;
    tx.set_command(tlm::TLM_WRITE_COMMAND); tx.set_address(uint64_t(csr_base_)+address);
    tx.set_data_ptr(data); tx.set_data_length(4); tx.set_streaming_width(4);
    tx.set_byte_enable_ptr(be); tx.set_byte_enable_length(4);
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    registers->b_transport(tx,delay); consume_delay(delay);
    return tx.get_response_status();
}
void HostDriver::transfer(bool write,uint32_t address,std::vector<unsigned char>& data) {
    tlm::tlm_generic_payload tx;
    tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND); tx.set_address(address);
    tx.set_data_ptr(data.data()); tx.set_data_length(static_cast<unsigned>(data.size()));
    tx.set_streaming_width(static_cast<unsigned>(data.size()));
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    memory->b_transport(tx,delay); consume_delay(delay);
    if(tx.is_response_error()) throw std::runtime_error("host DDR access failed");
}
void HostDriver::configure(unsigned width,unsigned height,uint32_t cmb,uint32_t refm,uint32_t nal) {
    for(auto item: {std::pair<uint32_t,uint32_t>{reg::SCON,0}, {reg::FMSIZE,(width<<16)|height},
        {reg::CMB,cmb},{reg::REFM,refm},{reg::NAL,nal},{reg::DFCON,0},{reg::SPARA2,0}})
        if(write(item.first,item.second)!=tlm::TLM_OK_RESPONSE) throw std::runtime_error("configuration failed");
    program_sequence();
}
void HostDriver::program_sequence() {
    const auto& p=programming;
    const unsigned batch=p.cmb_frames?p.cmb_frames:(p.frame_address_mode?1:p.frame_count);
    if(p.qp>63 || p.frame_count>65535 || batch>127 || p.gop_m>15 || p.gop_n>15 || p.log2_fn>15 || p.log2_poc>15)
        throw std::runtime_error("host register field overflow");
    const uint32_t s0=(p.qp<<21)|(unsigned(p.frame_address_mode)<<9)|0x100|(p.gop_n<<4)|p.gop_m;
    const uint32_t s1=(batch<<25)|(unsigned(p.force_log)<<24)|(p.log2_fn<<20)|(p.log2_poc<<16)|p.frame_count;
    if(write(reg::SPARA0,s0)!=tlm::TLM_OK_RESPONSE || write(reg::SPARA1,s1)!=tlm::TLM_OK_RESPONSE)
        throw std::runtime_error("sequence configuration failed");
}
ActivationResult HostDriver::wait_for_completion(const sc_core::sc_signal_in_if<bool>& irq,
        sc_core::sc_time timeout,sc_core::sc_time grace) {
    if(timeout<=sc_core::SC_ZERO_TIME || grace<sc_core::SC_ZERO_TIME) throw std::invalid_argument("invalid host timeout");
    ActivationResult result;
    sc_core::wait(sc_core::SC_ZERO_TIME); sc_core::wait(sc_core::SC_ZERO_TIME);
    const auto deadline=sc_core::sc_time_stamp()+timeout;
    while(!irq.read() && sc_core::sc_time_stamp()<deadline)
        sc_core::wait(deadline-sc_core::sc_time_stamp(),irq.value_changed_event());
    result.timed_out=!irq.read();
    // Preserve the status BEFORE disable/recovery; reads acknowledge the IRQ.
    for(unsigned i=0;i<result.registers.size();++i) result.registers[i]=read(i*4);
    result.status=result.registers[reg::STAT/4];
    if(write(reg::SCON,0)!=tlm::TLM_OK_RESPONSE) result.diagnostic="disable failed; ";
    const auto recovery_deadline=sc_core::sc_time_stamp()+grace;
    uint32_t status=read(reg::STAT);
    while((status&reg::BUSY) && sc_core::sc_time_stamp()<recovery_deadline) {
        sc_core::wait(std::min(sc_core::sc_time(10,sc_core::SC_NS),recovery_deadline-sc_core::sc_time_stamp()));
        status=read(reg::STAT);
    }
    result.quiescent=!(status&reg::BUSY);
    result.words=read(reg::STM_LEN);
    if(result.quiescent && result.words) {
        if(result.words>programming.nal_capacity_bytes/4) result.diagnostic+="reported length exceeds allocation; ";
        else {
            try {
                result.output.resize(size_t(result.words)*4);
                transfer(false,result.registers[reg::NAL/4],result.output);
            } catch(const std::exception& e) { result.output.clear(); result.diagnostic+=e.what(); }
        }
    }
    if(!result.quiescent) result.diagnostic+="BUSY after recovery grace; preserve buffers, reset required before reuse";
    last_result=result;
    return result;
}
void HostDriver::save_diagnostics(const std::string& directory) const {
    namespace fs=std::filesystem;
    fs::create_directories(directory);
    std::ofstream log(fs::path(directory)/"activation_status.txt");
    log<<"timed_out="<<last_result.timed_out<<"\nquiescent="<<last_result.quiescent
       <<"\nwords="<<last_result.words<<"\ndiagnostic="<<last_result.diagnostic<<'\n';
    for(unsigned i=0;i<last_result.registers.size();++i) log<<std::hex<<i*4<<"="<<last_result.registers[i]<<'\n';
    std::ofstream bytes(fs::path(directory)/"partial_output.bin",std::ios::binary);
    if(!last_result.output.empty()) bytes.write(reinterpret_cast<const char*>(last_result.output.data()),last_result.output.size());
    if(!log || !bytes) throw std::runtime_error("cannot save activation diagnostics");
}
}
