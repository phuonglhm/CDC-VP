#include <h264/inter/inter_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "../core/test_support.h"
#include "../core/interpolation_oracle.h"
#include <iostream>
#include <functional>
using namespace h264::inter;
using namespace sc_core;

struct Bench : sc_module {
    tlm_utils::simple_initiator_socket<Bench> socket{"socket"};
    sc_signal<bool, SC_MANY_WRITERS> reset_n{"reset_n"}, enable{"enable"};
    InterTlm model;
    h264::ResetDomain domain;
    Reference l0{List::L0,{10,0}}, l1{List::L1,{11,1}};
    std::string scenario;
    sc_event action_event;
    sc_event action_finished;
    sc_time action_delay;
    std::function<void()> action;
    bool failed = false, finished = false;
    bool action_pending = false;
    std::array<uint8_t,256> current{};
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name, std::string which, Options options)
        : sc_module(name), model("model",options), scenario(std::move(which)) {
        model.reset_n(reset_n); model.frame_enable(enable); socket.bind(model.target_socket);
        current.fill(50);
        SC_THREAD(run); SC_THREAD(helper);
    }
    void helper() {
        for (;;) {
            wait(action_event); const auto task=action; wait(action_delay);
            try { task(); } catch (const std::exception& e) { failed=true; std::cerr<<"helper: "<<e.what()<<'\n'; sc_stop(); }
            action_pending=false; action_finished.notify(SC_ZERO_TIME);
        }
    }
    void schedule(sc_time delay, std::function<void()> task) {
        require(!action_pending,"only one helper action outstanding");
        action_pending=true;
        action=std::move(task); action_delay=delay; action_event.notify(SC_ZERO_TIME);
    }
    void settle_action() { if (action_pending) wait(action_finished); }
    tlm::tlm_response_status transact(Operation operation, Extension& ext, uint8_t* bytes,
                                      unsigned count, sc_time incoming=SC_ZERO_TIME,
                                      h264::EpochExtension* supplied=nullptr, bool with_epoch=true) {
        tlm::tlm_generic_payload tx;
        tx.set_command(operation==Operation::Peek ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(operation)); tx.set_data_ptr(bytes); tx.set_data_length(count);
        tx.set_streaming_width(count); tx.set_extension(&ext);
        h264::EpochExtension epoch(domain);
        if (with_epoch) tx.set_extension(supplied ? supplied : &epoch);
        socket->b_transport(tx,incoming);
        const auto status=tx.get_response_status();
        require(!tx.is_dmi_allowed(),"DMI disabled");
        tx.clear_extension<Extension>(); tx.clear_extension<h264::EpochExtension>();
        return status;
    }
    void frame(bool fill=true) {
        reset_n=false; enable=false; wait(1,SC_NS);
        reset_n=true; wait(1,SC_NS); enable=true;
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        domain.release(); wait(SC_ZERO_TIME);
        model.retag(l0.list,l0.tag,32,32); model.retag(l1.list,l1.tag,32,32);
        current.fill(50);
        if (fill) { fill_all(l0); fill_all(l1); }
    }
    void fill_all(Reference ref) {
        for (auto plane : {Plane::Y,Plane::U,Plane::V}) {
            const unsigned n=plane==Plane::Y ? 32 : 16;
            model.refill(ref,plane,0,0,n,n,std::vector<uint8_t>(n*n,uint8_t(50+10*unsigned(plane))));
        }
    }
    Extension search(bool fractional=false) {
        Extension ext;
        ext.request.integer_candidates={{l0,{0,0},0}};
        if (fractional) ext.request.fractional_candidates={{{1,0},0},{{-1,2},1}};
        return ext;
    }
    Token evaluate_mode(bool fractional=false) {
        auto ext=search(fractional);
        require(transact(Operation::Evaluate,ext,current.data(),256)==tlm::TLM_OK_RESPONSE,"evaluate accepted");
        require(ext.decision.has_value() && ext.syntax.has_value(),"winner plus EEI returned");
        require(ext.decision->mode.winner.sad==0,"constant source predictor SAD");
        return ext.token;
    }
    void commit(Token token) {
        Extension ext; ext.token=token;
        require(transact(Operation::Commit,ext,nullptr,0)==tlm::TLM_OK_RESPONSE,"commit accepted");
        require(ext.syntax.has_value() && !model.done(),"commit syntax/no premature done");
    }
    void drain(Token token, bool delayed_chroma=false, MotionVector expected_mv={}) {
        commit(token);
        for (unsigned i=0;i<384;++i) {
            if (delayed_chroma && i==256) {
                schedule(sc_time(3,SC_NS),[&] {
                    require(model.progress().stage==Progress::Stage::Sample && model.progress().sample==256,"MC waits at U support");
                    require(!model.output() && !model.done(),"missing U cannot publish valid/done");
                    model.refill(l0,Plane::U,0,0,16,16,std::vector<uint8_t>(256,60));
                    model.refill(l0,Plane::V,0,0,16,16,std::vector<uint8_t>(256,70));
                });
            }
            Extension peek; peek.token=token; uint8_t pixel=0;
            require(transact(Operation::Peek,peek,&pixel,1)==tlm::TLM_OK_RESPONSE,"peek accepted");
            require(peek.sample && peek.sample->sequence==i && peek.sample->last==(i==383),"MC sample sequence/last");
            const unsigned plane=i<256 ? 0 : i<320 ? 1 : 2;
            require(unsigned(peek.sample->plane)==plane && pixel==50+plane*10,"Y/U/V predictor values");
            const int denominator=plane==0 ? 4 : 8;
            require(peek.sample->phase_x==unsigned((expected_mv.x%denominator+denominator)%denominator) &&
                    peek.sample->phase_y==unsigned((expected_mv.y%denominator+denominator)%denominator),"MC fractional phase metadata");
            require(!model.done() && model.output() && model.output()->sequence==i,"unaccepted sample held");
            if (i<256) {
                const unsigned block=i/16, lane=i%16;
                // Independent expected 4x4 block order in the enclosing MB.
                constexpr unsigned bx[16]={0,4,0,4,8,12,8,12,0,4,0,4,8,12,8,12};
                constexpr unsigned by[16]={0,0,4,4,0,0,4,4,8,8,12,12,8,8,12,12};
                require(peek.sample->block_index==block && peek.sample->x==bx[block]+lane%4 && peek.sample->y==by[block]+lane/4,"AVC transform order");
            }
            if (i%53==0 || i==383) {
                const auto held=*model.output(); wait(i==383 ? sc_time(5,SC_NS) : sc_time(1,SC_NS));
                require(!model.done() && model.output()->sequence==held.sequence && model.output()->value==held.value,"backpressure preserves last/data");
                Extension repeated; repeated.token=token; uint8_t again=0;
                require(transact(Operation::Peek,repeated,&again,1)==tlm::TLM_OK_RESPONSE && again==pixel && repeated.sample->sequence==i,"repeated peek idempotent");
                Extension wrong; wrong.token=token; wrong.accept_sequence=i+1;
                require(transact(Operation::Accept,wrong,&pixel,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong acceptance sequence rejected");
                require(model.output() && model.output()->sequence==i,"bad acceptance preserves valid");
            }
            Extension accept; accept.token=token; accept.accept_sequence=i;
            require(transact(Operation::Accept,accept,&pixel,1)==tlm::TLM_OK_RESPONSE,"sample accepted");
            require(accept.done==(i==383) && model.done()==(i==383),"done only after final handshake");
        }
        require(!model.busy() && !model.output(),"MC releases mode after final accept");
        Extension old; old.token=token; uint8_t pixel=0;
        require(transact(Operation::Peek,old,&pixel,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"completed token rejected");
    }
    void normal() {
        frame(false);
        model.refill(l0,Plane::Y,0,0,8,16,std::vector<uint8_t>(128,50));
        schedule(sc_time(1,SC_NS),[&] {
            require(model.progress().candidate==0 && model.progress().stage==Progress::Stage::Integer,"row-start phase held");
            const auto before=model.progress();
            // An unrelated list refill must not release selected-list support.
            model.refill(l1,Plane::Y,0,0,32,32,std::vector<uint8_t>(1024,50));
            auto busy=search();
            require(transact(Operation::Evaluate,busy,current.data(),256)==tlm::TLM_GENERIC_ERROR_RESPONSE,"overlapping transport rejected");
            wait(1,SC_NS);
            require(model.progress().candidate==before.candidate && model.progress().metadata->reference.list==List::L0,"unrelated list cannot advance phase/address metadata");
            model.refill(l0,Plane::Y,8,0,8,16,std::vector<uint8_t>(128,50));
        });
        auto ext=search();
        require(transact(Operation::Evaluate,ext,current.data(),256)==tlm::TLM_OK_RESPONSE,"row-start two column refill");
        drain(ext.token,true);
        // New frame: P -> B must not inherit List-1 residency.
        frame(false); fill_all(l0);
        schedule(sc_time(3,SC_NS),[&] {
            require(model.progress().candidate==1 && model.progress().metadata->reference.list==List::L1,"B startup waits on L1 tuple");
            require(!model.done() && !model.output(),"B readiness suppresses MC valid");
            fill_all(l1);
        });
        auto b=search(true); b.request.picture=Picture::B;
        b.request.integer_candidates[0].rate=100;
        b.request.integer_candidates.push_back({l1,{0,0},1});
        b.request.predicted_mv={-4,8};
        const auto begin=sc_time_stamp();
        require(transact(Operation::Evaluate,b,current.data(),256)==tlm::TLM_OK_RESPONSE,"dual-list B search");
        require(sc_time_stamp()>=begin+sc_time(3,SC_NS),"L1 delay reflected in timing");
        require(b.decision->mode.winner.candidate.reference.list==List::L1 && b.syntax->mvd.x==5 && b.syntax->mvd.y==-8,"B fractional winner and EEI alignment");
        drain(b.token,false,{1,0});
        frame(); const auto token=evaluate_mode(true); drain(token);
    }
    void rejection_tests() {
        frame(); const auto token=evaluate_mode();
        Extension premature; premature.token=token; uint8_t pixel=0;
        require(transact(Operation::Peek,premature,&pixel,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"peek before commit rejected");
        require(transact(Operation::Accept,premature,&pixel,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"accept before commit rejected");
        commit(token);
        Extension peek; peek.token=token;
        require(transact(Operation::Peek,peek,&pixel,1)==tlm::TLM_OK_RESPONSE,"owner sample prepared");
        h264::EpochExtension stale(domain); stale.generation=domain.generation+1;
        Extension accept; accept.token=token; accept.accept_sequence=0;
        require(transact(Operation::Accept,accept,&pixel,1,sc_time(50,SC_NS),&stale)==tlm::TLM_GENERIC_ERROR_RESPONSE,"stale epoch rejected on entry");
        require(model.output() && model.output()->sequence==0 && !model.done(),"stale request preserves current owner");
        auto invalid=peek; invalid.token.sequence++;
        require(transact(Operation::Peek,invalid,&pixel,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong token rejected");
        for (unsigned variant=0;variant<6;++variant) {
            tlm::tlm_generic_payload tx; Extension ext; ext.token=token; uint8_t mask=255;
            tx.set_extension(&ext); tx.set_address(uint64_t(Operation::Peek)); tx.set_command(tlm::TLM_READ_COMMAND);
            tx.set_data_ptr(&pixel); tx.set_data_length(1); tx.set_streaming_width(1);
            tlm::tlm_response_status expected=tlm::TLM_GENERIC_ERROR_RESPONSE;
            if (variant==0) { tx.set_address(99); expected=tlm::TLM_ADDRESS_ERROR_RESPONSE; }
            if (variant==1) { tx.set_command(tlm::TLM_WRITE_COMMAND); expected=tlm::TLM_COMMAND_ERROR_RESPONSE; }
            if (variant==2) { tx.set_byte_enable_ptr(&mask); expected=tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE; }
            if (variant==3) { tx.set_data_ptr(nullptr); expected=tlm::TLM_BURST_ERROR_RESPONSE; }
            if (variant==4) { tx.set_streaming_width(0); expected=tlm::TLM_BURST_ERROR_RESPONSE; }
            if (variant==5) tx.clear_extension<Extension>();
            sc_time delay=SC_ZERO_TIME; socket->b_transport(tx,delay);
            require(tx.get_response_status()==expected,"TLM malformed response type");
            require(model.output() && model.output()->value==pixel,"malformed request preserves held output");
            tx.clear_extension<Extension>();
        }
        require(transact(Operation::Accept,accept,&pixel,1)==tlm::TLM_OK_RESPONSE,"owner can accept after stale/malformed calls");
        reset_n=false; wait(1,SC_NS); reset_n=true; wait(1,SC_NS);
        auto after_reset=search();
        require(transact(Operation::Evaluate,after_reset,current.data(),256)==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset requires enable-low rearm");
        rejects([&]{model.retag(l0.list,l0.tag,32,32);},"inactive frame rejects cache import");
        frame(); require(evaluate_mode().generation!=token.generation,"rearmed frame uses a new generation");
        // EpochExtension is optional: exercise refill/service waits governed
        // solely by local reset/frame signals, and reject a duplicate Commit.
        frame(false);
        schedule(sc_time(3,SC_NS),[&]{fill_all(l0);});
        auto bare=search(true);
        require(transact(Operation::Evaluate,bare,current.data(),256,sc_time(2,SC_NS),nullptr,false)==tlm::TLM_OK_RESPONSE,"bare local-epoch refill and service wait");
        Extension c; c.token=bare.token;
        require(transact(Operation::Commit,c,nullptr,0,SC_ZERO_TIME,nullptr,false)==tlm::TLM_OK_RESPONSE,"bare commit");
        require(transact(Operation::Commit,c,nullptr,0,SC_ZERO_TIME,nullptr,false)==tlm::TLM_GENERIC_ERROR_RESPONSE,"duplicate commit rejected without losing owner");
        Extension p; p.token=bare.token;
        require(transact(Operation::Peek,p,&pixel,1,SC_ZERO_TIME,nullptr,false)==tlm::TLM_OK_RESPONSE,"bare sample latency");
        Extension a; a.token=bare.token; a.accept_sequence=0;
        require(transact(Operation::Accept,a,&pixel,1,SC_ZERO_TIME,nullptr,false)==tlm::TLM_OK_RESPONSE,"bare handshake after invalid commit");
    }
    void cancellations() {
        // Each operation that waits is cancelled, including the incoming-delay
        // boundary before winner creation or before a saved MC acceptance.
        for (unsigned site=0;site<7;++site) {
            frame(); Token token{}; uint8_t pixel=0;
            if (site>=3) { token=evaluate_mode(true); commit(token); }
            if (site==5 || site==6) {
                Extension p; p.token=token;
                require(transact(Operation::Peek,p,&pixel,1)==tlm::TLM_OK_RESPONSE,"prepare cancellation held sample");
            }
            if (site==0) model.retag(l0.list,l0.tag,32,32); // Missing SW support.
            if (site==4) { // MC waits on support after a freshly committed mode.
                // Chroma was not required for search. Retag/fill Y only BEFORE
                // evaluating the replacement mode, so its incarnation is valid.
                frame(false); model.refill(l0,Plane::Y,0,0,32,32,std::vector<uint8_t>(1024,50));
                token=evaluate_mode(); commit(token);
                for (unsigned i=0;i<256;++i) {
                    Extension p; p.token=token; transact(Operation::Peek,p,&pixel,1);
                    Extension a; a.token=token; a.accept_sequence=i; transact(Operation::Accept,a,&pixel,1);
                }
            }
            const auto before_generation=model.generation();
            const auto start=sc_time_stamp();
            sc_time cancel_after(2,SC_NS);
            if (site==2) cancel_after=sc_time(6,SC_NS); // IME done at 5ns; FME candidate active.
            if (site==3 || site==5) cancel_after=sc_time(500,SC_PS); // Sample/accept service 1ns.
            schedule(cancel_after,[&] {
                if (site==2 && scenario!="incoming")
                    require(model.progress().stage==Progress::Stage::Fractional && model.progress().metadata->mv.x==1,"FME cancellation retains fractional candidate metadata");
                if (scenario=="shared" || scenario=="incoming") domain.assert_reset();
                else if (scenario=="local") reset_n=false;
                else if (scenario=="disable") enable=false;
                else model.retag(l0.list,l0.tag,32,32);
            });
            tlm::tlm_response_status response;
            if (site<3) {
                auto e=search(site==2);
                response=transact(Operation::Evaluate,e,current.data(),256,scenario=="incoming"?sc_time(100,SC_NS):SC_ZERO_TIME);
                require(!e.decision,"cancelled search publishes no decision");
            } else {
                Extension e; e.token=token; e.accept_sequence=0;
                const auto op=(site==5 || site==6)?Operation::Accept:Operation::Peek;
                response=transact(op,e,&pixel,1,(site==6 || scenario=="incoming")?sc_time(100,SC_NS):SC_ZERO_TIME);
                require(!e.sample && !e.done,"cancelled MC publishes no sample/done");
            }
            require(response==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset/retag cancels active transport");
            require(sc_time_stamp()==start+cancel_after,"cancel wakes at event rather than old deadline");
            require(!model.output() && !model.done(),"cancel clears held output/done");
            if (scenario!="retag") require(model.generation()>before_generation && !model.cache().matches(l0),"epoch cancellation invalidates reference state");
        }
    }
    void incoming_snapshot() {
        frame(); auto ext=search();
        schedule(sc_time(2,SC_NS),[&] {
            current.fill(255); domain.changed.notify(SC_ZERO_TIME); // Benign same-epoch event.
        });
        const auto start=sc_time_stamp();
        require(transact(Operation::Evaluate,ext,current.data(),256,sc_time(10,SC_NS))==tlm::TLM_OK_RESPONSE,"incoming transaction finishes");
        require(ext.decision->mode.winner.sad==0,"current MB owned before incoming wait");
        require(sc_time_stamp()==start+sc_time(15,SC_NS),"benign notification preserves absolute service deadlines");
    }
    void partitions() {
        constexpr unsigned widths[7]={4,4,8,8,8,16,16}, heights[7]={4,8,4,8,16,8,16};
        constexpr unsigned bx[16]={0,4,0,4,8,12,8,12,0,4,0,4,8,12,8,12};
        constexpr unsigned by[16]={0,0,4,4,0,0,4,4,8,8,12,12,8,8,12,12};
        unsigned cases=0;
        for (unsigned seed : {20261010u,12345678u,87654321u}) for (bool b : {false,true}) {
            frame(false);
            const unsigned w=seed%2?64:48, h=seed%2?48:32;
            std::array<std::array<std::vector<uint8_t>,3>,2> images;
            for (unsigned list=0;list<2;++list) {
                const Reference ref=list?l1:l0; model.retag(ref.list,ref.tag,w,h);
                for (unsigned plane=0;plane<3;++plane) {
                    const unsigned scale=plane?2:1;
                    images[list][plane]=pattern(w/scale,h/scale,seed+list*89+plane*17);
                    model.refill(ref,Plane(plane),0,0,w/scale,h/scale,images[list][plane]);
                }
            }
            const Reference selected=b?l1:l0;
            const MotionVector target{-5,5};
            auto reader=[&](Plane plane) -> SampleReader {
                const int scale=plane==Plane::Y?1:2;
                return [&,plane,scale](int x,int y) {
                    return images[b?1:0][unsigned(plane)][size_t(std::clamp(y,0,int(h/scale)-1))*(w/scale)+std::clamp(x,0,int(w/scale)-1)];
                };
            };
            for (unsigned kind=0;kind<7;++kind) for (unsigned py=0;py<16;py+=heights[kind])
                for (unsigned px=0;px<16;px+=widths[kind]) {
                    Extension e; auto& r=e.request;
                    r.mb_x=seed%2?w-16:0; r.mb_y=seed%2?h-16:0;
                    r.partition={px,py,widths[kind],heights[kind]}; r.picture=b?Picture::B:Picture::P;
                    r.predicted_mv={8,-12};
                    if (b) r.integer_candidates.push_back({l0,{0,0},200000});
                    r.integer_candidates.push_back({selected,{-4,4},10});
                    r.integer_candidates.push_back({selected,{8,-8},200000});
                    r.fractional_candidates={{{-1,1},0},{{1,-1},17}};
                    const auto source=pattern(16,16,seed+cases);
                    std::copy(source.begin(),source.end(),current.begin());
                    std::vector<uint8_t> expected_y;
                    for (unsigned y=py;y<py+heights[kind];++y) for (unsigned x=px;x<px+widths[kind];++x) {
                        const auto value=oracle_luma(reader(Plane::Y),int(r.mb_x+x)*4+target.x,int(r.mb_y+y)*4+target.y);
                        current[y*16+x]=value; expected_y.push_back(value);
                    }
                    require(transact(Operation::Evaluate,e,current.data(),256,sc_time(3,SC_PS))==tlm::TLM_OK_RESPONSE,"partition oracle search accepted");
                    require(e.decision && e.decision->mode.winner.cost==0 && e.decision->predictor==expected_y,"partition winner predictor from independent oracle");
                    const auto& winner=e.decision->mode.winner.candidate;
                    require(winner.reference.list==selected.list && winner.reference.tag==selected.tag && winner.mv.x==target.x && winner.mv.y==target.y,"nonconstant fractional MV/list/slot alignment");
                    require(e.syntax && e.syntax->mvd.x==-13 && e.syntax->mvd.y==17,"negative MV EEI metadata");
                    require(std::all_of(e.decision->residual.begin(),e.decision->residual.end(),[](int16_t v){return v==0;}),"oracle-matched source residual zero");
                    const auto token=e.token;
                    const unsigned mb_x=r.mb_x, mb_y=r.mb_y;
                    commit(token);
                    unsigned sequence=0;
                    const unsigned total=widths[kind]*heights[kind]*3/2;
                    for (unsigned plane=0;plane<3;++plane) {
                        const unsigned scale=plane?2:1;
                        for (unsigned block=0;block<(plane?4u:16u);++block) {
                            const unsigned block_x=plane?(block%2)*4:bx[block];
                            const unsigned block_y=plane?(block/2)*4:by[block];
                            for (unsigned y=block_y;y<block_y+4;++y) for (unsigned x=block_x;x<block_x+4;++x) {
                                if (x<px/scale || x>=(px+widths[kind])/scale || y<py/scale || y>=(py+heights[kind])/scale) continue;
                                const unsigned ax=mb_x/scale+x, ay=mb_y/scale+y;
                                const int phase=plane?8:4;
                                const auto expected=plane?oracle_chroma(reader(Plane(plane)),int(ax)*8+target.x,int(ay)*8+target.y):
                                    oracle_luma(reader(Plane::Y),int(ax)*4+target.x,int(ay)*4+target.y);
                                Extension peek; peek.token=token; uint8_t value=0;
                                require(transact(Operation::Peek,peek,&value,1)==tlm::TLM_OK_RESPONSE && value==expected,"TLM MC numeric oracle for partition/plane");
                                require(peek.sample && peek.sample->plane==Plane(plane) && peek.sample->block_index==block && peek.sample->x==ax && peek.sample->y==ay,"partial chroma/transform coordinate metadata");
                                require(peek.sample->sequence==sequence && peek.sample->last==(sequence+1==total) &&
                                        peek.sample->phase_x==unsigned((target.x%phase+phase)%phase) && peek.sample->phase_y==unsigned(target.y%phase),"fractional sample phase/last metadata");
                                if (sequence%11==0) {
                                    wait(sc_time(1+(sequence%31),SC_PS));
                                    require(model.output() && model.output()->value==value && model.output()->sequence==sequence && !model.done(),"random stall preserves sample and completion");
                                    Extension repeated; repeated.token=token; uint8_t again=0;
                                    require(transact(Operation::Peek,repeated,&again,1)==tlm::TLM_OK_RESPONSE && again==value && repeated.sample->sequence==sequence,"nonconstant repeat Peek");
                                    Extension wrong; wrong.token=token; wrong.accept_sequence=sequence;
                                    uint8_t corrupted=value^1;
                                    require(transact(Operation::Accept,wrong,&corrupted,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong predictor byte rejected");
                                }
                                Extension accept; accept.token=token; accept.accept_sequence=sequence++;
                                require(transact(Operation::Accept,accept,&value,1)==tlm::TLM_OK_RESPONSE && accept.done==(sequence==total),"partition sample handshake/done");
                            }
                        }
                    }
                    require(sequence==total && model.done() && !model.busy(),"all partition YUV samples accepted");
                    ++cases;
                }
        }
        require(cases==246,"all 41 partition positions x P/B x 3 seeds");
        std::cout<<"Partition regression: "<<cases<<" cases; 3 seeds, negative fractional MV, nonconstant YUV\n";
    }
    void boundaries() {
        const bool zero=scenario=="boundary-zero";
        unsigned cases=0, accepted=0, cancelled=0, equal_accepted=0, equal_cancelled=0;
        for (unsigned kind=0;kind<4;++kind) for (unsigned site=0;site<12;++site) for (int offset : {-1,0,1}) {
            frame(); Token token{}; uint8_t value=0;
            if (site>=5) token=evaluate_mode();
            if (site>=6) commit(token);
            const unsigned cursor=site==10?383:0;
            if (site>=8) {
                for (unsigned i=0;i<=cursor;++i) {
                    Extension p; p.token=token;
                    require(transact(Operation::Peek,p,&value,1)==tlm::TLM_OK_RESPONSE,"prepare boundary predictor");
                    if (i<cursor) {
                        Extension a; a.token=token; a.accept_sequence=i;
                        require(transact(Operation::Accept,a,&value,1)==tlm::TLM_OK_RESPONSE,"prepare final sample boundary");
                    }
                }
            }
            uint64_t boundary_ps=10000, completion_ps=10000;
            if (site<5) {
                completion_ps+=zero?0:(site>=3?10000:5000);
                if (!zero) boundary_ps+=site==1?4000:site==2?5000:site==3?9000:site==4?10000:0;
            } else if (site==6 || site==7 || (site>=8 && site<=10)) {
                completion_ps+=zero?0:1000;
                if (!zero && (site==7 || site==9 || site==10)) boundary_ps+=1000;
            }
            const uint64_t cancel_ps=uint64_t(int64_t(boundary_ps)+offset);
            const auto start=sc_time_stamp();
            schedule(sc_time::from_value(cancel_ps),[&,kind] {
                if (kind==0) domain.assert_reset();
                else if (kind==1) reset_n=false;
                else if (kind==2) enable=false;
                else model.retag(l0.list,l0.tag,32,32);
            });
            Extension ext; ext.token=token; ext.accept_sequence=cursor;
            Operation op;
            if (site<5) {
                ext=search(site>=3); if (site>=3) ext.request.fractional_candidates.resize(1);
                op=Operation::Evaluate;
            } else if (site==5) op=Operation::Commit;
            else if (site==6 || site==7 || site==11) op=Operation::Peek;
            else op=Operation::Accept;
            const auto response=transact(op,ext,op==Operation::Evaluate?current.data():op==Operation::Commit?nullptr:&value,
                op==Operation::Evaluate?256:op==Operation::Commit?0:1,sc_time(10,SC_NS));
            const bool ok=response==tlm::TLM_OK_RESPONSE;
            if (cancel_ps<completion_ps) require(!ok,"reset before completion must cancel");
            else if (cancel_ps>completion_ps) require(ok,"completion before reset must succeed");
            else require(ok || response==tlm::TLM_GENERIC_ERROR_RESPONSE,"same-time completion/cancel must be a valid serial outcome");
            if (ok) {
                ++accepted;
                require(sc_time_stamp()==start+sc_time::from_value(completion_ps),"accepted boundary retains full deadline");
                if (op==Operation::Evaluate) require(ext.decision.has_value(),"accepted boundary publishes decision");
                if (op==Operation::Peek) require(ext.sample.has_value(),"accepted boundary publishes sample");
                if (op==Operation::Accept) require(ext.sample && ext.done==(site==10),"only accepted final sample publishes done");
            } else {
                ++cancelled;
                require(response==tlm::TLM_GENERIC_ERROR_RESPONSE && !ext.decision && !ext.sample && !ext.done,"cancelled boundary suppresses output/completion");
                require(sc_time_stamp()==start+sc_time::from_value(cancel_ps),"cancelled boundary wakes at event");
                require(!model.output() && !model.done(),"cancelled boundary invalidates held data");
            }
            if (cancel_ps==completion_ps) { if (ok) ++equal_accepted; else ++equal_cancelled; }
            settle_action(); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); ++cases;
        }
        require(cases==144 && accepted>0 && cancelled>0,"boundary matrix complete");
        std::cout<<"Boundary regression: "<<cases<<" cases, "<<accepted<<" accepted, "<<cancelled<<" cancelled; equal-time "
                 <<equal_accepted<<" accepted / "<<equal_cancelled<<" cancelled\n";
    }
    void run() {
        try {
            if (scenario=="normal" || scenario=="zero") normal();
            else if (scenario=="partitions" || scenario=="partitions-zero") partitions();
            else if (scenario=="boundaries" || scenario=="boundary-zero") boundaries();
            else if (scenario=="stale") rejection_tests();
            else if (scenario=="incoming") { incoming_snapshot(); cancellations(); }
            else cancellations();
            finished=true;
            std::cout<<"TLM "<<scenario<<": "<<checks<<" checks PASS at "<<sc_time_stamp()<<'\n';
        } catch (const std::exception& e) { failed=true; std::cerr<<scenario<<": "<<e.what()<<" at "<<sc_time_stamp()<<'\n'; }
        sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    sc_set_time_resolution(1,SC_PS);
    const std::string scenario=argc>1?argv[1]:"normal";
    Options options;
    if (scenario=="zero" || scenario=="partitions-zero" || scenario=="boundary-zero")
        options.candidate_latency=options.compare_latency=options.sample_latency=options.accept_latency=SC_ZERO_TIME;
    Bench bench("bench",scenario,options); sc_start();
    if (!bench.finished) std::cerr<<"Test did not reach completion (possible simulation starvation)\n";
    return bench.failed || !bench.finished ? 1 : 0;
}
