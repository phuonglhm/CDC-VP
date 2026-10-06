# Tích hợp Bus System vào CDC-VP

Bản phát triển trong `Bus System` dự kiến thay thế component `components/FX1_Components/bus`. Thay đổi tại CMake cấp cha và wiring platform do đội tích hợp thực hiện; test trong thư mục này chỉ đọc source CDC-VP.

## CMake

```cmake
# components/CMakeLists.txt của repo cha
add_subdirectory(FX1_Components/bus)
# CMake của platform
target_link_libraries(your_platform PRIVATE cdc::components::bus_system)
```

Component không gọi `project()` khi được nhúng và dùng lại `SystemC::systemc`. `BUS_SYSTEM_BUILD_TESTS` mặc định theo `CDC_BUILD_TESTS` ở lần configure đầu; standalone theo `BUILD_TESTING`. Cache CMake đã có sẽ giữ giá trị cũ cho tới khi override.

Nếu root project là `cdc-vp`, thư viện tham gia `cdc-components-targets`. Parent khác muốn dùng cùng export set phải đặt `BUS_SYSTEM_USE_CDC_EXPORT=ON`; parent chịu trách nhiệm `install(EXPORT ...)`. Standalone tạo package `bus-system`, dùng bằng `find_package(bus-system CONFIG REQUIRED)`.

## API thay thế

API cũ `fabric.target`, `fabric.rom.socket`, `fabric.pp0[i]` được thay bằng named binding:

```cpp
auto cfg = bus::BusConfig::fx1();
// Trong constructor/top, trước elaboration:
bus::BusSystem fabric{"fabric", cfg};
cpu1.socket.bind(fabric.initiator("CPU1"));
cpu2.socket.bind(fabric.initiator("CPU2"));
isp_idma.socket.bind(fabric.initiator("ISP_IDMA"));
isp_odma.socket.bind(fabric.initiator("ISP_ODMA"));
fabric.target("BootROM").bind(bootrom.target);
fabric.target("CLINT").bind(clint.target);
fabric.target("PLIC").bind(plic.target);
fabric.target("UART").bind(uart.target);
fabric.target("MEMCTL_DDR").bind(memctl.target);
```

Tên member socket của IP phụ thuộc model thực tế (`target`, `socket`, ...). Cấu hình data width chỉ chấp nhận 32 bit. Address width mặc định 32; thay map/latency trong cfg trước khi tạo bus.

| Path | Đường đi |
|---|---|
| `SysBus1Axi` | SYSBUS_1 → IP |
| `SysBus0Axi` | SYSBUS_1 → SYSBUS_0 → IP |
| `Peribus1Apb` | SYSBUS_1 → A2P_PP1 → PERIBUS_1 → IP |
| `Peribus0Apb` | SYSBUS_1 → SYSBUS_0 → A2P_PP0 → PERIBUS_0 → IP |

Muốn giữ AES/QSPI theo sơ đồ cũ, thêm target với `SysBus0Axi` và map được platform phê duyệt. Muốn bật CSR TBD, điền base/size và `enabled=true` trên descriptor đã tồn tại; không thêm descriptor trùng tên. Mọi enabled target phải bind; initiator chưa dùng có thể bỏ trống. Thêm tên master/IP và reorder descriptor không cần sửa core.

## Hợp đồng với IP và CPU

- IP nhận local offset và chịu trách nhiệm register side effects, read-only, IRQ/reset và byte enables. `memory_tlm` trong CDC hiện không thực hiện byte-enable masking; bài test masked accesses dùng target hỗ trợ strobes riêng.
- Blocking transport phải từ process có thể `wait`. Bus tiêu thụ annotated delay trước khi trả về. CPU quantum keeper cần tích hợp theo hợp đồng này để không cộng latency hai lần.
- DMI/nonblocking chưa hỗ trợ; CPU backend yêu cầu hai chức năng này cần adapter hoặc mở rộng bus.
- APB chỉ 1/2/4 byte; CPU truy cập 8 byte tới CLINT/CSR sẽ bị từ chối. Platform phải xác nhận access width của driver hoặc triển khai split có quy tắc side effects; test bus hiện không chứng minh driver CLINT/PLIC hoạt động.
- `transport_dbg` phục vụ ELF loader, không áp APB beat/alignment. Bus không khởi tạo BSS; loader/startup chịu trách nhiệm.
- FIFO phục vụ theo request, không weighted QoS hoặc fairness theo số byte. Chia sẻ upstream output vẫn tuần tự hóa cả nhánh.

## Kiểm tra tích hợp và package

```bash
export SYSTEMC_HOME=/path/to/systemc-install
export CDC_VP_SOURCE_DIR=/path/to/CDC-VP
bash scripts/check_cdc.sh
```

Script build harness từ `tests/cdc_integration`, dùng `memory_tlm` và ELF loader thật, install shared export set rồi build consumer riêng. Output nằm trong `build-cdc/` và `build-installed-consumer/` của Bus System.

Standalone install/consumer:

```bash
cmake -S . -B build-package -DCMAKE_PREFIX_PATH="$SYSTEMC_HOME" -DBUS_SYSTEM_BUILD_TESTS=OFF
cmake --build build-package --parallel 4
cmake --install build-package --prefix "$PWD/build-package/install"
cmake -S tests/installed_consumer -B build-consumer -DCMAKE_PREFIX_PATH="$PWD/build-package/install;$SYSTEMC_HOME"
cmake --build build-consumer --parallel 2
ctest --test-dir build-consumer --output-on-failure
```

Đặt `BUILD_SHARED_LIBS=ON` khi muốn build chính thư viện bus thành shared. SystemC phải phù hợp ABI. Không dùng chung build directory giữa Linux/WSL và Windows native.

Kiểm tra platform tiếp theo: map và binding IP thật, driver MMIO, byte enable, CPU/DMA workload và boot firmware. Đây là bước riêng của đội platform, không được suy ra từ regression component.
