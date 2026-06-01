// =============================================================
// Testbench — verifies the same 5 assertions as your C++ sim:
//   R8  ($t0) = 10
//   R9  ($t1) = 5
//   R10 ($t2) = 15
//   R11 ($t3) = 15   (loaded via SW then LW)
//   DataMem[16] = 15 (SW wrote correctly)
// =============================================================
`timescale 1ns/1ps

module tb_mips;

reg clk, rst_n;
integer cycle, pass_count, fail_count;

mips_pipeline dut (
    .clk   (clk),
    .rst_n (rst_n)
);

// 10 ns clock
initial clk = 0;
always #5 clk = ~clk;

// -------------------------------------------------------
// Task: check register value
// -------------------------------------------------------
task check_reg;
    input [4:0]  reg_num;
    input [31:0] expected;
    input [79:0] name_str;   // up to 10 ASCII chars packed
    begin
        if (dut.regfile[reg_num] === expected) begin
            $display("  PASS  %s (R%0d) = %0d", name_str, reg_num, expected);
            pass_count = pass_count + 1;
        end else begin
            $display("  FAIL  %s (R%0d): got %0d, expected %0d",
                     name_str, reg_num, dut.regfile[reg_num], expected);
            fail_count = fail_count + 1;
        end
    end
endtask

// -------------------------------------------------------
// Task: check data memory (byte address)
// -------------------------------------------------------
task check_mem;
    input [31:0] byte_addr;
    input [31:0] expected;
    begin
        if (dut.dmem[byte_addr/4] === expected) begin
            $display("  PASS  DataMem[%0d] = %0d", byte_addr, expected);
            pass_count = pass_count + 1;
        end else begin
            $display("  FAIL  DataMem[%0d]: got %0d, expected %0d",
                     byte_addr, dut.dmem[byte_addr/4], expected);
            fail_count = fail_count + 1;
        end
    end
endtask

// -------------------------------------------------------
// Stimulus
// -------------------------------------------------------
initial begin
    $dumpfile("mips_pipeline.vcd");
    $dumpvars(0, tb_mips);

    pass_count = 0;
    fail_count = 0;
    cycle      = 0;

    // Reset for 2 cycles
    rst_n = 0;
    repeat(2) @(posedge clk);
    rst_n = 1;

    // Run 20 cycles — enough for 6 instructions + pipeline drain
    repeat(20) @(posedge clk);

    // -----------------------------------------------
    // Self-check — same 5 assertions as C++ sim
    // -----------------------------------------------
    $display("\n=== Verilog Pipeline Self-Check ===");
    check_reg(8,  32'd10, "$t0");
    check_reg(9,  32'd5,  "$t1");
    check_reg(10, 32'd15, "$t2");
    check_reg(11, 32'd15, "$t3");
    check_mem(32'd16, 32'd15);

    $display("\n-----------------------------------");
    $display("  Cycles run  : %0d", cycle);
    $display("  Passed      : %0d", pass_count);
    $display("  Failed      : %0d", fail_count);
    if (fail_count == 0)
        $display("  ALL TESTS PASSED");
    else
        $display("  SOME TESTS FAILED — open mips_pipeline.vcd in GTKWave");
    $display("===================================\n");

    $finish;
end

// Cycle counter
always @(posedge clk) if (rst_n) cycle = cycle + 1;

// Timeout guard
initial begin
    #5000;
    $display("TIMEOUT");
    $finish;
end

endmodule