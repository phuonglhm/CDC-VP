# NPU (SAURIA) — Hướng dẫn cấu hình cho đội Software

Tài liệu này giải thích **cách cấu hình NPU**, **ý nghĩa từng tham số**, và **tác động** của
chúng lên hành vi/hiệu năng. Dùng kèm demo pack: mỗi `demo` case là một cấu hình cụ thể chạy
end-to-end (`make demo CASE=<name>`).

Có 3 tầng cấu hình, từ trừu tượng → phần cứng:

1. **Shape workload** (đội SW mô tả bài toán): `Bw Bh d s Cin Cw Ch Cout Xused Yused preload`.
2. **HW version** (kích thước phần cứng, cố định khi build): `X, Y, IDX widths, MEM depths`.
3. **Thanh ghi config SAURIA** (giá trị nạp vào NPU lúc chạy): `o_xlim, o_xstep, ...`.

Đội SW chủ yếu làm việc ở tầng 1 (mô tả workload) và tầng 3 (nạp thanh ghi). Tầng 3 sinh tự động
từ tầng 1+2 — bằng **Python SAURIA** (`config_helper.get_sauria_regs`, dùng khi verify offline)
hoặc bằng **thư viện driver C thuần** `driver/libsauria_cfg.h` (không cần Python lúc chạy — xem mục 4b).

---

## 1) Shape workload — `Bw Bh d s Cin Cw Ch Cout Xused Yused preload`

Truyền cho `run_shape.sh` / `capture_case.sh`. Đây là mô tả một lớp conv/GeMM.

| Ký hiệu   | Tên                 | Ý nghĩa                                                | Tác động                                                                              |
| --------- | ------------------- | ------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| `Bw`,`Bh` | Kernel width/height | Kích thước cửa sổ trọng số (vd 3×3). `1×1` = GeMM/MVM. | Tăng → nhiều vị trí kernel/contexts hơn → nhiều chu kỳ, K (độ sâu tích chập) lớn hơn. |
| `d`       | Dilation            | Giãn kernel (khoảng cách lấy mẫu activation).          | Chỉ đổi cách sinh địa chỉ activation (`o_ystep = A_w*d`); không đổi số MAC.           |
| `s`       | Stride              | Bước trượt cửa sổ trên activation.                     | Giảm kích thước output; đổi `o_til_x/ystep` (bộ sinh địa chỉ strided).                |
| `Cin`     | Input channels      | Số kênh vào (chiều tích chập K với 1×1).               | = độ sâu contraction/output-element → tỉ lệ thuận số MAC & chu kỳ nạp.                |
| `Cw`,`Ch` | Output spatial W/H  | Kích thước không gian output.                          | `Cw*Ch*Cout` = tổng phần tử output.                                                   |
| `Cout`    | Output channels     | Số kênh ra (= số cột trọng số).                        | `Cout > Xused` ⇒ **nhiều output-tile** (external tiling).                             |
| `Xused`   | Cột PE dùng         | Số cột systolic array được kích hoạt (≤ `X`).          | Ánh xạ vào `o_cols_active`; cột không dùng bị gate.                                   |
| `Yused`   | Hàng PE dùng        | Số hàng systolic array được kích hoạt (≤ `Y`).         | Ánh xạ vào `o_rows_active`; hàng không dùng đọc về 0.                                 |
| `preload` | Preload/accumulate  | `1` = cộng dồn vào C sẵn có trong DRAM; `0` = ghi đè.  | Bật ⇒ kết quả cuối = `initial_C + compute`; tắt ⇒ = `compute`.                        |

**Ví dụ:**

- `1 1 1 1 64 8 1 16 16 8 1` → 1×1 conv (MVM), Cin=64, output 8×1×16, dùng 16×8 PE, preload ON.
- `1 1 1 1 256 64 1 64 64 64 1` → GeMM 64×64, K=256.
- `3 3 1 2 16 8 4 32 32 8 1` → conv 3×3 stride 2.
- `3 3 1 1 16 8 4 64 32 8 1` → Cout=64 > Xused=32 ⇒ 2 output-tile.

---

## 2) HW version — kích thước phần cứng (cố định khi build)

Chọn qua `SAURIA_VERSION` khi sinh stimuli, và qua `EVAL_X/EVAL_Y` + `IDX_FLAGS` + cờ dtype khi
build testbench. **Bắt buộc khớp nhau**, sai → decode config bậy. Có **8 version** = 3 geometry ×
3 kiểu dữ liệu (xem bảng ở mục 2b). Mọi hằng số của mỗi version nằm trong **một nguồn duy nhất**:
`sauria_targets.csv` (sinh ra `sauria_targets.h` bằng `tools/gen_targets.py`).

| Tham số                             | Ý nghĩa                                           | Tác động                                                                                                                                        |
| ----------------------------------- | ------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `X`,`Y`                             | Kích thước systolic array (cột × hàng).           | Peak = `X*Y` MAC/chu kỳ. Giới hạn `Xused ≤ X`, `Yused ≤ Y`.                                                                                     |
| `IA_W`,`IB_W`,`OC_W`                | Bit rộng activation / weight / partial-sum.       | Tùy kiểu dữ liệu (xem 2b): int8=8/8/32, FP16=16/16/16, int16=16/16/64. Ảnh hưởng độ rộng bus + kích thước phần tử DRAM.                         |
| `MEMA/B/C_DEPTH`                    | Độ sâu SRAM A(act)/B(wei)/C(psum).                | Giới hạn kích thước tile chứa on-chip.                                                                                                          |
| `IFM_IDX_W`,`WEI_IDX_W`,`PSM_IDX_W` | Độ rộng chỉ số packed-config của bộ sinh địa chỉ. | **PHẢI** khớp version (xem bảng 2b). Sai → `ncontexts` decode loạn. Lưu ý: FP16_8x16 = 15/15/15 (khác int8_8x16 = 15/16/14 vì MEMB_DEPTH khác). |

Truyền độ rộng khi build: `IDX_FLAGS="-DSAURIA_ACT_IDX_W=.. -DSAURIA_WEI_IDX_W=.. -DSAURIA_OUT_IDX_W=.."`
(mỗi demo case lưu sẵn trong `case.env`).

---

## 2b) Kiểu dữ liệu (datatype) — INT8 / FP16 / INT16

Kiểu dữ liệu là **thuộc tính build của phần cứng** (SAURIA cố định `IA_W`/`OP_TYPE` theo bản), KHÔNG
phải cờ runtime. Không có 1 binary chạy cả 3 kiểu — có **3 build**, chọn qua VERSION + cờ dtype.

| Version                           | X×Y                  | IA/IB/OC (bit) | IDX (A/W/O)                    | In/Out byte | Verify       | Cờ dtype build                                                                        |
| --------------------------------- | -------------------- | -------------- | ------------------------------ | ----------- | ------------ | ------------------------------------------------------------------------------------- |
| `int8_8x16` / `_32x32` / `_64x64` | 16×8 / 32×32 / 64×64 | 8/8/32         | 15/16/14 · 17/17/16 · 18/18/17 | 1 / 4       | **exact 0**  | (mặc định)                                                                            |
| `FP16_8x16` / `_32x32` / `_64x64` | 16×8 / 32×32 / 64×64 | 16/16/16       | 15/15/15 · 17/17/16 · 18/18/17 | 2 / 2       | **≤ 16 ULP** | `-DNPU_FP16 -DNPU_DTYPE_IN=fp16_t -DNPU_DTYPE_OUT=fp16_t -DNPU_DTYPE_PSUM=float`      |
| `int16_8x16` / `_32x32`           | 16×8 / 32×32         | 16/16/64       | 15/16/14 · 17/17/16            | 2 / 8       | **exact 0**  | `-DNPU_INT16 -DNPU_DTYPE_IN=int16_t -DNPU_DTYPE_OUT=int64_t -DNPU_DTYPE_PSUM=int64_t` |

- **INT8 / INT16**: số nguyên nên khớp bit tuyệt đối (0 mismatch). INT16 dùng accumulator int64 (OC_W=64) tránh tràn.
- **FP16**: phép cộng dấu phẩy động không kết hợp → thứ tự cộng của mảng khác tham chiếu; verify dùng **dung sai ULP** (`NPU_FP16_ULP_TOL`, mặc định 16). Đây KHÔNG phải fudge — bug logic sẽ lệch hàng trăm ULP/NaN. Quan sát: 8×16 exact, 32×32 max 1 ULP, 64×64 max 8 ULP.
- Cờ dtype đã gói sẵn trong `case.env` của mỗi demo và trong `run_shape.sh` (theo tên VERSION).

---

## 3) Thanh ghi config SAURIA (nạp lúc chạy)

Pipeline sinh ra một danh sách `(địa chỉ, giá trị)` (file `GoldenStimuli.txt`) mà đội SW nạp
tuần tự vào NPU qua giao diện config-AXI, rồi phát xung `start`. Các trường (theo vùng):

### Vùng CONTROL

| Trường       | Ý nghĩa                                            | Tác động                                                |
| ------------ | -------------------------------------------------- | ------------------------------------------------------- |
| `o_incntlim` | `Bw*Bh*Cin - 1` = độ sâu tích chập K trừ 1.        | Quyết định số MAC/phần tử output; `mvm_k = incntlim+1`. |
| `o_act_reps` | Số lần lặp lại nạp activation (`ceil(Cin/Xused)`). | Nhiều lần nếu Cin > số cột dùng.                        |
| `o_wei_reps` | Số lần lặp nạp trọng số (`ceil(Cw/Yused)*Ch`).     | Tăng theo kích thước kernel/output.                     |
| `o_thres`    | Ngưỡng negligence (approx computing).              | Bỏ qua MAC nhỏ hơn ngưỡng (nhánh approx).               |

### Vùng ACTIVATION (bộ sinh địa chỉ đọc activation từ SRAM A)

| Trường                  | Ý nghĩa                                                 | Tác động                              |
| ----------------------- | ------------------------------------------------------- | ------------------------------------- |
| `o_xlim`,`o_xstep`      | Giới hạn/bước quét theo trục X (hàng output).           | Điều khiển vòng quét cột output.      |
| `o_ylim`,`o_ystep`      | Giới hạn/bước theo trục Y; `o_ystep = A_w*d`.           | `d` (dilation) vào đây.               |
| `o_chlim`,`o_chstep`    | Giới hạn/bước theo kênh (`A_w*A_h`).                    | Quét theo chiều Cin.                  |
| `o_til_x/y lim/step`    | Tham số vòng lặp tile (external tiling) + `s` (stride). | Strided & multi-tile nằm ở đây.       |
| `o_Dil_pat` (64-bit)    | Bitmap mẫu dilation.                                    | Chọn phần tử activation nào tham gia. |
| `o_rows_active` (Y-bit) | Bitmap hàng PE hoạt động (từ `Yused`).                  | Hàng tắt → output đọc về 0.           |
| `o_loc_woffs`           | Offset cục bộ nạp weight.                               | Căn chỉnh nạp trọng số.               |

### Vùng WEIGHT

| Trường                     | Ý nghĩa                                       | Tác động                                    |
| -------------------------- | --------------------------------------------- | ------------------------------------------- |
| `o_wlim`,`o_wstep`         | Giới hạn/bước quét weight (`Cout*Bw*Bh*Cin`). | Tổng lượng weight nạp.                      |
| `o_klim`,`o_kstep`         | Tham số nạp theo cột SRAM B.                  | Tối ưu khi weight "aligned".                |
| `o_til_klim`,`o_til_kstep` | Vòng lặp tile theo Cout / Xused.              | Multi-tile weight.                          |
| `o_cols_active` (X-bit)    | Bitmap cột PE hoạt động (từ `Xused`).         | Cột tắt bị gate.                            |
| `o_waligned`               | Cờ weight thẳng hàng → tối ưu feeder.         | Bật nếu `Cin%SRAMB_N==0 && Xused==SRAMB_N`. |

### Vùng OUTPUT (PSM ghi partial-sum ra SRAM C)

| Trường               | Ý nghĩa                                             | Tác động                                                                            |
| -------------------- | --------------------------------------------------- | ----------------------------------------------------------------------------------- |
| `o_ncontexts`        | Số "context" (vị trí kernel / nhóm output) mỗi lần. | Bố cục output: `elem = x*(ncontexts*Y)+ctx*Y+y`.                                    |
| `o_cxlim`,`o_cxstep` | `= Y_used + SRAMC_N` / bước ghi C.                  | **Lưu ý:** `o_cxlim` KHÔNG phải số cột output — đừng dùng làm vòng lặp đếm phần tử. |
| `o_cklim`,`o_ckstep` | Giới hạn/bước theo chiều tích chập khi ghi.         | Cộng dồn partial-sum.                                                               |
| `o_til_c* `          | Tham số vòng lặp tile khi ghi output.               | Multi-tile output.                                                                  |
| `o_inactive_cols`    | Số cột không dùng (`X - Xused`).                    | Gate cột thừa.                                                                      |
| `o_preload_en`       | Bật cộng dồn C sẵn có.                              | = `preload` ở tầng 1.                                                               |

---

## 4) Luồng cấu hình chuẩn cho đội Software

1. Mô tả workload → shape (tầng 1). Chọn HW version khớp (tầng 2).
2. Sinh stimuli: `run_shape.sh "<shape>" EVAL_X EVAL_Y <version>` (gọi pipeline SAURIA) →
   ra `GoldenStimuli.txt` (config), `initial_dram.txt`, `gold_dram.txt`.
3. Nạp DRAM ban đầu (activation + weight + C preload) vào bộ nhớ NPU.
4. Nạp lần lượt `(addr, value)` từ `GoldenStimuli.txt` vào thanh ghi config.
5. Phát xung `start`, chờ `done`.
6. Đọc SRAM C, cộng preload nếu `preload_en`, so với `gold_dram`.

Đóng gói toàn bộ (2)–(6) trong 1 lệnh: `make demo CASE=<name>` (build tb_demo đúng geometry,
in tiêu đề test + `[RUNTIME CONFIG]` + bảng so khớp + khối `PERFORMANCE`).

### 4b) Sinh config bằng C — KHÔNG cần Python (cho driver/framework)

Tầng 3 ở trên sinh bằng Python (`config_helper`). Để driver **tự sinh `controller_args` lúc chạy
mà không nhúng Python**, dùng thư viện C thuần trong `driver/`:

- `driver/libsauria_cfg.h` — `sauria_encode_controller_args(desc, target, dram_bases)` → toàn bộ
  `controller_args` (args[0..21] tiling + args[22..] core config). **Đã verify bit-exact** với
  Python trên 9 shape × 3 datatype (`make enctest`).
- `driver/libsauria_mem.h` — `sauria_assemble_dram(...)` đóng gói tensor A/B/C → ảnh DRAM (bit-exact,
  `make memtest`); `sauria_unpack_output(...)` đọc kết quả.
- `driver/sauria_run.h` — API gộp: `sauria_prepare(target, desc, A, B, Cpre)` → `{controller_args,
initial_dram, offsets}`; sau khi core chạy: `sauria_read_output(...)`.
- Hằng số version lấy từ `sauria_find_target("int8_32x32")` (đọc `sauria_targets.h`).

**Golden cũng sinh được bằng C** (không cần Python/torch): `driver/sauria_golden.h` →
`sauria_reference_conv(A, B, Cpre, shape, target)` tính output mong đợi (reference conv). INT
bit-exact với SAURIA; FP16 trong cùng dung sai ULP. Đã đối chiếu với `gold_dram` của **mọi** demo
case bằng `make goldtest` (thuần C, không Python). → Cả **config + đóng gói DRAM + golden** đều có
bản C, nên một testcase có thể **sinh và kiểm hoàn toàn trên máy đích, không cần SAURIA Python**.

**Đã khép kín hoàn toàn:** `GoldenStimuli.txt` (chuỗi lệnh ghi thanh ghi) cũng sinh được bằng C —
`driver/sauria_stim.h` (`make stimtest` bit-exact với file đã ship). `tools/gen_case.cpp`
(`make selftest`) **sinh một testcase mới hoàn chỉnh — input + config + GoldenStimuli + golden —
thuần C** rồi chạy qua `tb_demo` PASS, không cần Python. (Golden hiện phủ regime `Cw == Yused`;
trường hợp W-splitting `Cw > Yused` cần map lại thứ tự output — việc tiếp theo.)

Tạo case mới bằng C:

```bash
make selftest                                  # sinh + chạy vài case mẫu (Python-free)
/tmp/sauria_gencase <version> "<shape>" <out_dir> [seed]   # sinh 1 case tùy ý
```

---

## 5) Đo hiệu năng (throughput / utilization)

Mỗi demo in khối **PERFORMANCE (cycle basis)**:

- `Execution cycles (start → done)`: số chu kỳ clock từ xung start tới `i_done_std` (cửa sổ compute).
- `Total MACs = output_elements × K` (K = `mvm_k` = `incntlim+1`).
- `Throughput (MAC/cycle) = Total MACs / cycles`.
- `Array utilization = Throughput / (X*Y peak)`.
- `Output throughput = output_elements / cycles`.

Đây là **cơ sở ban đầu**. Con số utilization còn thấp vì `cycles` bao gồm cả nạp config/preload
và fill/drain của systolic array. Bước tinh chỉnh tiếp theo: tách các pha (config load, DMA in,
compute, DMA out) và mô hình hoá overlap để ra utilization thực của mảng PE.

> **Trạng thái verify (cập nhật 2026-07-02):** **TẤT CẢ 11/11 demo PASS THẬT** — 5 INT8 (8×16,
> 32×32, 64×64, strided, multi-tile; exact 0), 3 FP16 (8×16/32×32/64×64; trong dung sai ULP),
> 2 INT16 (8×16/32×32; exact 0), 1 conv 5×5. Bug bố cục SRAM-C khi `EVAL_Y > 8` (và 7 bug khác)
> **đã fix** — xem "8 FIX" trong `HANDOFF.md`. Số cycles/throughput hợp lệ và kết quả đúng đắn
> đã được xác nhận trên mọi hình học/kiểu dữ liệu ở trên. Chạy lại toàn bộ: `make check`.
