#include "../core/test_support.h"
#include "../core/interpolation_oracle.h"
#include "sw_dma.h"
#include "axi_master_bridge.h"
#include "ddr_model.h"
using namespace h264::inter;

namespace {
void pump(h264::H264Arb& arb, h264::AxiMasterBridge& bridge, h264::DdrModel& ddr) {
    unsigned service_steps=0;
    while (arb.busy()) {
        require(++service_steps<10000,"bounded DMA queue service");
        const auto owner=arb.pick(); require(arb.acquire(owner),"DMA grant");
        const auto request=arb.granted_request(); bridge.begin(request,ddr);
        auto status=h264::AxiProgress::Pending;
        for (unsigned steps=0;status==h264::AxiProgress::Pending;++steps) {
            require(steps<10000,"bounded bridge service"); status=bridge.advance();
            require(arb.granted()==owner,"owner held through delayed read response");
        }
        const auto response=bridge.response();
        arb.complete(status==h264::AxiProgress::Success,response.data);
    }
}
bool import_ready(h264::SwDma& sw, h264::RefList list, ReferenceCache& cache, Reference ref) {
    if (!sw.ready(list)) return false;
    for (unsigned plane=0;plane<3;++plane) {
        const auto& pixels=sw.plane(list,plane);
        require(std::all_of(pixels.valid.begin(),pixels.valid.end(),[](uint8_t b){return b!=0;}),"all imported support is valid");
        cache.fill(ref,Plane(plane),pixels.x,pixels.y,pixels.width,pixels.height,pixels.pixels);
    }
    return true;
}
}
int main(int argc,char** argv) {
    try {
        const unsigned bus=argc>1?unsigned(std::stoul(argv[1])):32;
        const h264::CodedDims dims{48,32}; constexpr uint64_t base=0x1ff0;
        h264::H264Arb arb; h264::DdrModel ddr(65536);
        h264::AxiBridgeConfig service; service.data_width_bits=bus;
        service.arready_delay=2; service.rvalid_delay=3; service.r_beat_gap=1; service.consumer_delay=2;
        h264::AxiMasterBridge bridge(service);
        h264::SwDma sw(arb,dims,{32,32}); sw.set_refm_base(base);
        ReferenceCache cache;
        for (unsigned frame=0;frame<3;++frame) {
            for (unsigned slot=0;slot<3;++slot) {
                const auto pixels=pattern(48,48,20261010+frame*97+slot*53); // 48*48 = YUV frame bytes.
                ddr.write(base+slot*2304,pixels.data(),pixels.size());
            }
            Reference refs[2]={{List::L0,{frame*2+100,frame%3}},{List::L1,{frame*2+101,(frame+1)%3}}};
            for (unsigned list=0;list<2;++list) {
                sw.set_ref_slot(h264::RefList(list),int(refs[list].tag.slot),refs[list].tag.picture);
                cache.retag(refs[list].list,refs[list].tag,48,32);
                require(!sw.ready(h264::RefList(list)),"picture switch invalidates SW list");
            }
            const bool b_picture=frame==1;
            for (unsigned my=0;my<2;++my) for (unsigned mx=0;mx<3;++mx) {
                const size_t before=arb.log().size();
                sw.fill_window(h264::RefList::List0,mx,my);
                if (b_picture) sw.fill_window(h264::RefList::List1,mx,my);
                const auto& issued=arb.log().entries();
                if (mx==1 && my==0) {
                    // x=16 reuses the previous window's x=16..31 overlap.
                    for (size_t i=before;i<issued.size();++i) {
                        const auto refslot=(issued[i].addr-base)/2304;
                        const auto offset=(issued[i].addr-base-refslot*2304);
                        const unsigned stride=offset<1536?48:24;
                        const unsigned plane_offset=offset<1536?0:offset<1920?1536:1920;
                        require((offset-plane_offset)%stride >= (stride==48?32u:16u),"overlap reused: DMA reads new right strip only");
                    }
                }
                if (!sw.ready(h264::RefList::List0)) {
                    const auto phase=sw.phase();
                    require(!sw.advance_phase(h264::RefList::List0,base+mx*16),"SW phase stalls before response");
                    require(sw.phase()==phase,"phase not incremented during refill");
                    require(!import_ready(sw,h264::RefList::List0,cache,refs[0]),"incomplete DMA never imported");
                }
                pump(arb,bridge,ddr);
                require(sw.fme_mc_may_start(b_picture),"P/B reference readiness satisfied");
                require(import_ready(sw,h264::RefList::List0,cache,refs[0]),"import completed L0 window");
                if (b_picture) require(import_ready(sw,h264::RefList::List1,cache,refs[1]),"import completed L1 window");
                require(sw.advance_phase(h264::RefList::List0,base+mx*16),"held SW request advances on residency");
                Request r; r.mb_x=mx*16; r.mb_y=my*16; r.picture=b_picture?Picture::B:Picture::P;
                r.integer_candidates={{refs[0],{0,0},0}};
                if (b_picture) r.integer_candidates.push_back({refs[1],{0,0},0});
                r.fractional_candidates={{{1,1},0},{{-1,-1},0}};
                const auto selected=refs[b_picture?1:0];
                std::array<uint8_t,256> source{};
                const auto refbase=base+selected.tag.slot*2304;
                for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x)
                    source[y*16+x]=ddr.read_byte(refbase+(r.mb_y+y)*48+r.mb_x+x);
                const auto decision=evaluate(cache,r,source);
                require(decision.mode.winner.sad==0 && decision.mode.winner.candidate.reference.list==selected.list,"SW-backed exact matching reference winner");
                for (const auto& sample : sample_layout(decision.mode)) {
                    const unsigned stride=sample.plane==Plane::Y?48:24;
                    const unsigned offset=sample.plane==Plane::Y?0:sample.plane==Plane::U?1536:1920;
                    const auto expected=ddr.read_byte(refbase+offset+sample.y*stride+sample.x);
                    require(compensate_sample(cache,decision.mode,sample)==expected,"MC matches reconstructed DDR reference");
                }
            }
        }
        for (const auto& segment : bridge.issued_log())
            require(segment.addr/4096==(segment.addr+(segment.len+1ull)*segment.size_bytes-1)/4096,"SW AXI bursts respect 4KiB");
        // Failed SW reads cannot mark the Inter cache ready; retry can recover.
        Reference failed_ref{List::L0,{900,2}};
        sw.set_ref_slot(h264::RefList::List0,2,900); sw.fill_window(h264::RefList::List0,0,0);
        cache.retag(failed_ref.list,failed_ref.tag,48,32);
        auto fault=service; fault.drop_rresp=true; h264::AxiMasterBridge bad_bridge(fault);
        pump(arb,bad_bridge,ddr);
        require(sw.failed(h264::RefList::List0) && !sw.ready(h264::RefList::List0),"failed RRESP prevents SW readiness");
        require(!import_ready(sw,h264::RefList::List0,cache,failed_ref),"failed DMA cannot publish cache residency");
        rejects([&]{(void)cache.sample(failed_ref,Plane::Y,0,0);},"failed DMA leaves Inter support absent");
        sw.fill_window(h264::RefList::List0,0,0); pump(arb,bridge,ddr);
        require(import_ready(sw,h264::RefList::List0,cache,failed_ref),"successful retry publishes support");
        // Old outstanding reads are drained after switching reference identity.
        sw.set_ref_slot(h264::RefList::List0,0,901); sw.fill_window(h264::RefList::List0,0,0);
        sw.set_ref_slot(h264::RefList::List0,1,902); sw.fill_window(h264::RefList::List0,0,0);
        Reference fresh{List::L0,{902,1}}; cache.retag(fresh.list,fresh.tag,48,32);
        pump(arb,bridge,ddr); require(import_ready(sw,h264::RefList::List0,cache,fresh),"stale DMA completions do not satisfy new tag");
        for (unsigned x=0;x<32;++x)
            require(cache.sample(fresh,Plane::Y,int(x),0)==ddr.read_byte(base+2304+x),"retag output belongs to new slot");
        std::cout<<"SW DMA integration bus "<<bus<<": "<<checks<<" checks; 3 P/B/P pictures, 18 MBs, errors/retag/overlap PASS\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
