# Test coverage và phần cần bổ sung

Theo [spec mapping](SPEC_MAPPING.md); evidence trong [VALIDATION](../VALIDATION.md).

## Tests đã triển khai

| CTest name | Coverage thực thi |
|---|---|
| h264_inter_sad_cache_search | 41 positions × 2 seeds × 128 trials; max SAD/wide rate/overflow; cache isolation/immutability; all partitions × 12 sources, independent IME/FME/MC oracle; EEI/ties/stale modes |
| h264_inter_interpolation_oracle | 3 seeds × 6 patterns; 16 luma phases trên -2..17 coordinates; 64 chroma phases ở interior/borders; direct 2D/table oracle và six-tap/diagonal anchors |
| h264_inter_limits | 40 geometry/MV cases: 16×16, QCIF, 720p, 1920×1088, 4096×4096; extreme signed MV và inward FME; 4096 integer/64 fractional candidates; invalid geometry/plane/MC/search, atomic refill và retag giữa search |
| h264_inter_sw_dma_32/64/128 | Actual SwDma/H264Arb/AxiMasterBridge/DdrModel; 48×32, P/B/P × 6 MBs; overlap, successful reads, RRESP fault/retry, stale outstanding reads, 4KiB split, MC vs DDR |
| h264_inter_contract_normal | 8+8 columns, unrelated L1/busy calls, delayed U, P/B/P, fractional winner/phase/EEI, independent transform order, repeat Peek/bad Accept/final stall |
| h264_inter_contract_zero | Cùng normal scenarios với zero service latency |
| h264_inter_contract_shared/local/disable/retag | 7 sites: absent SW, IME service, FME service, MC sample service, U support, Accept service, Accept incoming; wake đúng cancel event, không stale output/done |
| h264_inter_contract_incoming | Source snapshot trước delay, benign notification giữ deadline; shared cancel trong incoming search/Peek/Accept |
| h264_inter_contract_stale | Stale-at-entry giữ owner; invalid token/address/command/byte-enable/pointer/streaming/missing-extension; Peek/Accept trước Commit; duplicate Commit; reset rearm và optional EpochExtension/local-only waits |
| h264_inter_contract_partitions/partitions-zero | Mỗi profile: 3 seeds × P/B × 41 positions = 246 cases; fractional winner, EEI/MVD, oracle luma/chroma và independent transform order; stall/bad Accept/final handshake |
| h264_inter_contract_boundaries/boundary-zero | Mỗi profile: 4 cancel kinds × 12 sites × {-1,0,+1} ps = 144 cases; response deadline, no stale output/done và actor completion; timing thường và zero service |

Core-only có 3 tests; TLM không DMA có 15; full suite có 18. Timeout contract/
integration 30 giây, limits 60 giây. SystemC driver phải thực sự hoàn thành; starvation không
được tính PASS. Assertion counts không phải tỷ lệ coverage của toàn HAS.

Geometry tests dùng sparse corner fixture để kiểm tra clamping và arithmetic;
không phải encode đầy đủ các resolution này. Deadline bằng đúng cancellation
cho phép thứ tự tuần tự hợp lệ của SystemC; kiểm tra consistency của kết quả,
không áp đặt priority RTL chưa được spec định nghĩa. Gcov trên 7 production .cpp:
412/414 dòng, 627/831 branch outcomes đã chạy; xem VALIDATION và raw evidence.

## Tests còn mở

- SISLAB golden cho exact candidate priority/rate, autonomous partition/mode,
  MVP, P-skip/B-direct và bi/weighted prediction.
- Actual TQ/entropy/DF adapters và reconstructed feedback nhiều frame.
- QCIF byte comparison, PSNR và 601-frame qualification.
- Full-frame/resolution và search-window workloads theo encoder policy thực tế;
  geometry/MV regression hiện dùng các case biên, không duyệt toàn bộ miền input.
- Mọi simultaneous-event order và per-cycle reset priority của RTL; deadline
  regression hiện kiểm tra các vị trí/profile mô tả ở trên.
- RTL retiming/cycle/filter-internal assertions; hiện kiểm tra stage/candidate/
  sample metadata ở mức transaction.
- Performance report đủ resolution, I/P/B, search, AXI, DF, clock, cycles/stalls.
- Các branch outcomes chưa chạy, bao gồm compiler exception/unwind paths;
  không suy ra 100% spec coverage từ line coverage.

Full encoder signoff cần evidence riêng cho các mục còn mở.
