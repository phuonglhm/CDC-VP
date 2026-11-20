// axi_mem_ideal.sv -- byte memory behind a 128-bit AXI4 slave port, no latency model (first read beat one cycle after the AR is taken,
// one beat per cycle, write response one cycle after the last W beat): the same kind of ideal memory the hardware bench uses
// (axi_mem_slave). INCR bursts of 16-byte beats only. Read data comes from the beat address aligned down to 16 bytes, a write
// stores the bytes selected by wstrb. Up to QD bursts are queued on each of AR and AW.
module axi_mem_ideal #(
    parameter MEMB = 1 << 18,
    parameter QD   = 16,
    parameter RLAT = 0,      // extra cycles before the first read beat of a burst
    parameter WLAT = 0       // extra cycles before the write response
) (
    input  logic         clk,
    input  logic         rstn,
    input  logic [31:0]  araddr,
    input  logic [7:0]   arlen,
    input  logic         arvalid,
    output logic         arready,
    output logic [127:0] rdata,
    output logic         rlast,
    output logic         rvalid,
    input  logic         rready,
    input  logic [31:0]  awaddr,
    input  logic [7:0]   awlen,
    input  logic         awvalid,
    output logic         awready,
    input  logic [127:0] wdata,
    input  logic [15:0]  wstrb,
    input  logic         wlast,
    input  logic         wvalid,
    output logic         wready,
    output logic         bvalid,
    input  logic         bready
);
    logic [7:0] mem [0:MEMB-1];

    // read side
    logic [31:0] arq_a [0:QD-1];
    logic [7:0]  arq_l [0:QD-1];
    int arq_n;
    logic        r_act;
    logic [31:0] r_addr;
    logic [8:0]  r_left;
    int          r_wait;

    assign arready = (arq_n < QD);
    assign rvalid  = r_act && (r_wait == 0);
    assign rlast   = rvalid && (r_left == 1);
    always_comb begin
        rdata = '0;
        for (int i = 0; i < 16; i++) rdata[8*i +: 8] = mem[({r_addr[31:4], 4'd0} + i) % MEMB];
    end

    always_ff @(posedge clk) begin
        if (!rstn) begin
            arq_n <= 0; r_act <= 1'b0; r_wait <= 0; r_left <= 0; r_addr <= '0;
        end else begin
            int n;
            n = arq_n;
            if (arvalid && arready) begin
                arq_a[n] = araddr; arq_l[n] = arlen; n = n + 1;
            end
            if (r_act) begin
                if (r_wait > 0) r_wait <= r_wait - 1;
                else if (rready) begin
                    r_addr <= {r_addr[31:4], 4'd0} + 32'd16;
                    r_left <= r_left - 1;
                    if (r_left == 1) r_act <= 1'b0;
                end
            end else if (n > 0) begin
                r_act <= 1'b1; r_addr <= arq_a[0]; r_left <= {1'b0, arq_l[0]} + 9'd1; r_wait <= RLAT;
                for (int i = 0; i < QD - 1; i++) begin arq_a[i] = arq_a[i+1]; arq_l[i] = arq_l[i+1]; end
                n = n - 1;
            end
            arq_n <= n;
        end
    end

    // write side
    logic [31:0] awq_a [0:QD-1];
    logic [7:0]  awq_l [0:QD-1];
    int awq_n;
    logic        w_act;
    logic [31:0] w_addr;
    int          bq_n;
    int          b_wait;

    assign awready = (awq_n < QD);
    assign wready  = w_act || (awq_n > 0);
    assign bvalid  = (bq_n > 0) && (b_wait == 0);

    always_ff @(posedge clk) begin
        if (!rstn) begin
            awq_n <= 0; w_act <= 1'b0; bq_n <= 0; b_wait <= 0; w_addr <= '0;
        end else begin
            int n, bn;
            n = awq_n; bn = bq_n;
            if (awvalid && awready) begin
                awq_a[n] = awaddr; awq_l[n] = awlen; n = n + 1;
            end
            if (bvalid && bready) begin bn = bn - 1; b_wait <= WLAT; end
            else if (bq_n > 0 && b_wait > 0) b_wait <= b_wait - 1;
            if (wvalid && wready) begin
                logic [31:0] a;
                if (w_act) a = w_addr;
                else begin a = awq_a[0]; end
                for (int i = 0; i < 16; i++)
                    if (wstrb[i]) mem[({a[31:4], 4'd0} + i) % MEMB] <= wdata[8*i +: 8];
                if (wlast) begin
                    w_act <= 1'b0; bn = bn + 1;
                    if (!w_act) begin
                        for (int i = 0; i < QD - 1; i++) begin awq_a[i] = awq_a[i+1]; awq_l[i] = awq_l[i+1]; end
                        n = n - 1;
                    end else begin
                        // burst already popped when it started
                    end
                end else begin
                    w_addr <= {a[31:4], 4'd0} + 32'd16;
                    if (!w_act) begin
                        w_act <= 1'b1;
                        for (int i = 0; i < QD - 1; i++) begin awq_a[i] = awq_a[i+1]; awq_l[i] = awq_l[i+1]; end
                        n = n - 1;
                    end
                end
            end
            awq_n <= n; bq_n <= bn;
        end
    end
endmodule
