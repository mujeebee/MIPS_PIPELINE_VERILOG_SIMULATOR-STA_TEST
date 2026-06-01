// =============================================================
// ALU — matches your C++ switch(ALUOp) in execute()
// =============================================================
module alu (
    input  wire [31:0] a,
    input  wire [31:0] b,
    input  wire [5:0]  op,
    output reg  [31:0] result,
    output wire        zero      // a == b (used by BEQ)
);
    always @(*) begin
        case (op)
            6'h20: result = a + b;                                    // ADD
            6'h22: result = a - b;                                    // SUB
            6'h24: result = a & b;                                    // AND
            6'h25: result = a | b;                                    // OR
            6'h2A: result = ($signed(a) < $signed(b)) ? 32'd1 : 32'd0; // SLT
            default: result = 32'd0;
        endcase
    end
    // BEQ checks equality of the two ORIGINAL operands, not the SUB result
    assign zero = (a == b);
endmodule

// =============================================================
// Forwarding Unit — mirrors your forward() lambda in execute()
// Priority: EX/MEM > MEM/WB
// fwd: 2'b00 = register file,  2'b10 = EX/MEM,  2'b01 = MEM/WB
// =============================================================
module forwarding_unit (
    input  wire [4:0] IDEX_rs,
    input  wire [4:0] IDEX_rt,
    input  wire [4:0] EXMEM_rd,
    input  wire       EXMEM_RegWrite,
    input  wire [4:0] MEMWB_rd,
    input  wire       MEMWB_RegWrite,
    output reg  [1:0] fwd_a,
    output reg  [1:0] fwd_b
);
    always @(*) begin
        // --- Forward A (rs) ---
        if (EXMEM_RegWrite && EXMEM_rd != 0 && EXMEM_rd == IDEX_rs)
            fwd_a = 2'b10;
        else if (MEMWB_RegWrite && MEMWB_rd != 0 && MEMWB_rd == IDEX_rs)
            fwd_a = 2'b01;
        else
            fwd_a = 2'b00;

        // --- Forward B (rt) ---
        if (EXMEM_RegWrite && EXMEM_rd != 0 && EXMEM_rd == IDEX_rt)
            fwd_b = 2'b10;
        else if (MEMWB_RegWrite && MEMWB_rd != 0 && MEMWB_rd == IDEX_rt)
            fwd_b = 2'b01;
        else
            fwd_b = 2'b00;
    end
endmodule