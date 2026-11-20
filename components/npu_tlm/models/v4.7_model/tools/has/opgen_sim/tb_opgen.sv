// tb_opgen.sv -- bench for redma_top of neo_lite (revision 2): the tensor-operation register file (redma_opgen_top) and the one-run
// register file (conf_regs) through the one AXI4-Lite port, against an ideal 128-bit AXI memory. One clock: one cycle = one system
// clock of the ReDMA. Commands come from cmds.txt (one word per command):
//   W <addr> <data>        AXI-Lite write (addresses are offsets inside the DMA block: 0x00..0x3F conf_regs, 0x80.. opgen)
//   R <addr>               AXI-Lite read, prints "RD <addr> <data> <resp>"
//   S                      write opgen CTRL = 1 (start); prints "T_START <cycle of the B response>"
//   P                      poll opgen CTRL until done (bit 2), then clear it (CTRL = 2); prints "T_POLL <cycle>"
//   L <raddr> <waddr> <btt> <wz>   one-run descriptor through conf_regs (as redma_driver.sv); wz = 1 writes zeros (CONTROL bit 8)
//   Q                      write the memory to dump.hex and finish
// Log lines (all with the cycle number): SEG (one segment accepted by redma_seg_glue), SEGEND (b_engine: every B response of the
// segment returned), T_DONE (opgen retired the descriptor), T_LEG (conf_regs writer interrupt).
`timescale 1ns/1ps
module tb_opgen;
    localparam OG = 32'h80;
    logic clk = 1'b0, rstn = 1'b0;
    always #5 clk = ~clk;
    longint cyc = 0;
    always @(posedge clk) cyc <= cyc + 1;

    logic [31:0] l_awaddr, l_wdata, l_araddr, l_rdata;
    logic [3:0]  l_wstrb;
    logic [1:0]  l_bresp, l_rresp;
    logic l_awvalid = 0, l_awready, l_wvalid = 0, l_wready, l_bvalid, l_bready = 1, l_arvalid = 0, l_arready, l_rvalid, l_rready = 1;

    wire [31:0] ar_addr, aw_addr;
    wire [7:0]  ar_len, aw_len;
    wire [2:0]  ar_size, aw_size, ar_prot, aw_prot;
    wire [1:0]  ar_burst, aw_burst;
    wire        ar_valid, ar_ready, aw_valid, aw_ready, ar_lock, aw_lock;
    wire [3:0]  ar_cache, aw_cache, ar_qos, aw_qos, ar_region, aw_region;
    wire [0:0]  ar_id, aw_id;
    wire [127:0] r_data, w_data;
    wire [15:0] w_strb;
    wire        r_valid, r_ready, r_last, w_valid, w_ready, w_last, b_valid, b_ready;
    wire [1:0]  r_resp = 2'b00, b_resp = 2'b00;
    wire        rintr, wintr;

    redma_top #(
        .AXI_LITE_ADDR_WIDTH(32), .AXI_ADDR_WIDTH(32), .AXI_DATA_WIDTH(128), .AXI_ID_WIDTH(1),
        .AXI_MAX_ARLEN(255), .AXI_MAX_AWLEN(255), .READER_FIFO_LEN(2), .WRITER_FIFO_LEN(2),
        .AXI_READER_ADDR_OFFSET('0), .AXI_WRITER_ADDR_OFFSET('0), .ELM_BITS(8), .SYNC_AW_W(0),
        .MAX_OUTSTANDING_READS(8), .MAX_OUTSTANDING_WRITES(8)
    ) dut (
        .clk(clk), .rstn(rstn),
        .io_control_aw_awaddr(l_awaddr), .io_control_aw_awprot(3'b000), .io_control_aw_awvalid(l_awvalid), .io_control_aw_awready(l_awready),
        .io_control_w_wdata(l_wdata), .io_control_w_wstrb(l_wstrb), .io_control_w_wvalid(l_wvalid), .io_control_w_wready(l_wready),
        .io_control_b_bresp(l_bresp), .io_control_b_bvalid(l_bvalid), .io_control_b_bready(l_bready),
        .io_control_ar_araddr(l_araddr), .io_control_ar_arprot(3'b000), .io_control_ar_arvalid(l_arvalid), .io_control_ar_arready(l_arready),
        .io_control_r_rdata(l_rdata), .io_control_r_rresp(l_rresp), .io_control_r_rvalid(l_rvalid), .io_control_r_rready(l_rready),
        .m_axi_ar_arid(ar_id), .m_axi_ar_araddr(ar_addr), .m_axi_ar_arlen(ar_len), .m_axi_ar_arsize(ar_size), .m_axi_ar_arburst(ar_burst),
        .m_axi_ar_arlock(ar_lock), .m_axi_ar_arcache(ar_cache), .m_axi_ar_arprot(ar_prot), .m_axi_ar_arqos(ar_qos), .m_axi_ar_region(ar_region),
        .m_axi_ar_arvalid(ar_valid), .m_axi_ar_arready(ar_ready),
        .m_axi_r_rid(1'b0), .m_axi_r_rdata(r_data), .m_axi_r_rresp(r_resp), .m_axi_r_rlast(r_last), .m_axi_r_rvalid(r_valid), .m_axi_r_rready(r_ready),
        .m_axi_aw_awid(aw_id), .m_axi_aw_awaddr(aw_addr), .m_axi_aw_awlen(aw_len), .m_axi_aw_awsize(aw_size), .m_axi_aw_awburst(aw_burst),
        .m_axi_aw_awlock(aw_lock), .m_axi_aw_awcache(aw_cache), .m_axi_aw_awprot(aw_prot), .m_axi_aw_awqos(aw_qos), .m_axi_aw_region(aw_region),
        .m_axi_aw_awvalid(aw_valid), .m_axi_aw_awready(aw_ready),
        .m_axi_w_wdata(w_data), .m_axi_w_wstrb(w_strb), .m_axi_w_wlast(w_last), .m_axi_w_wvalid(w_valid), .m_axi_w_wready(w_ready),
        .m_axi_b_bid(1'b0), .m_axi_b_bresp(b_resp), .m_axi_b_bvalid(b_valid), .m_axi_b_bready(b_ready),
        .reader_intr(rintr), .writer_intr(wintr)
    );

    axi_mem_ideal #(.MEMB(1 << 18)) u_mem (
        .clk(clk), .rstn(rstn),
        .araddr(ar_addr), .arlen(ar_len), .arvalid(ar_valid), .arready(ar_ready),
        .rdata(r_data), .rlast(r_last), .rvalid(r_valid), .rready(r_ready),
        .awaddr(aw_addr), .awlen(aw_len), .awvalid(aw_valid), .awready(aw_ready),
        .wdata(w_data), .wstrb(w_strb), .wlast(w_last), .wvalid(w_valid), .wready(w_ready),
        .bvalid(b_valid), .bready(b_ready)
    );

    // ---- monitors
    always @(posedge clk) if (rstn) begin
        if (dut.g_tensor.seg_valid && dut.g_tensor.seg_ready)
            $display("SEG %0d %0d %h %h %0d %h %0d", cyc, dut.g_tensor.seg_fill, dut.g_tensor.seg_raddr, dut.g_tensor.seg_waddr,
                     dut.g_tensor.seg_len, dut.g_tensor.seg_fill_val, dut.g_tensor.seg_last);
        if (dut.set_writer_intr && dut.tensor_active) $display("SEGEND %0d", cyc);
        if (dut.g_tensor.glue_drained) $display("T_DONE %0d", cyc);
        if (dut.set_writer_intr && !dut.tensor_active) $display("T_LEG %0d", cyc);
    end

    // ---- AXI-Lite master
    task automatic cfg_w(input [31:0] a, input [31:0] d);
        bit aw_d = 0, w_d = 0;
        l_awaddr <= a; l_awvalid <= 1'b1; l_wdata <= d; l_wstrb <= 4'hF; l_wvalid <= 1'b1;
        while (!(aw_d && w_d)) begin
            @(posedge clk);
            if (l_awvalid && l_awready && !aw_d) begin aw_d = 1; l_awvalid <= 1'b0; end
            if (l_wvalid && l_wready && !w_d) begin w_d = 1; l_wvalid <= 1'b0; end
        end
        do @(posedge clk); while (!l_bvalid);
    endtask

    task automatic cfg_r(input [31:0] a, output [31:0] d, output [1:0] rsp);
        bit ar_d = 0;
        l_araddr <= a; l_arvalid <= 1'b1;
        while (!ar_d) begin
            @(posedge clk);
            if (l_arvalid && l_arready) begin ar_d = 1; l_arvalid <= 1'b0; end
        end
        do @(posedge clk); while (!l_rvalid);
        d = l_rdata; rsp = l_rresp;
    endtask

    integer f, n;
    reg [63:0] op;
    reg [31:0] a0, a1, a2, a3, rd;
    reg [1:0]  rr;
    longint    t0;

    initial begin
        $readmemh("mem_init.hex", u_mem.mem);
        f = $fopen("cmds.txt", "r");
        repeat (8) @(posedge clk);
        rstn <= 1'b1;
        repeat (4) @(posedge clk);
        while (!$feof(f)) begin
            n = $fscanf(f, "%s", op);
            if (n != 1) break;
            case (op)
                "W": begin n = $fscanf(f, "%h %h", a0, a1); cfg_w(a0, a1); end
                "R": begin n = $fscanf(f, "%h", a0); cfg_r(a0, rd, rr); $display("RD %h %h %0d", a0, rd, rr); end
                "S": begin cfg_w(OG, 32'd1); $display("T_START %0d", cyc); end
                "P": begin
                    rd = 0;
                    while (!rd[2]) cfg_r(OG, rd, rr);
                    $display("T_POLL %0d", cyc);
                    cfg_w(OG, 32'd2);
                end
                "L": begin
                    n = $fscanf(f, "%h %h %h %h", a0, a1, a2, a3);
                    cfg_w(32'h04, 32'd2);
                    cfg_w(32'h0C, 32'd3);
                    cfg_w(32'h10, a0);
                    cfg_w(32'h20, a1);
                    cfg_w(32'h30, a2);
                    t0 = cyc;
                    cfg_w(32'h00, a3[0] ? 32'h102 : 32'h003);
                    $display("T_LEGSTART %0d", cyc);
                    while (!wintr) @(posedge clk);
                    cfg_w(32'h0C, 32'd3);
                end
                "Q": begin $writememh("dump.hex", u_mem.mem); $display("END %0d", cyc); $finish; end
                default: $display("BADCMD %s", op);
            endcase
        end
        $writememh("dump.hex", u_mem.mem);
        $display("END %0d", cyc);
        $finish;
    end

    initial begin
        #(64'd400000000);
        $display("TIMEOUT");
        $finish;
    end
endmodule
