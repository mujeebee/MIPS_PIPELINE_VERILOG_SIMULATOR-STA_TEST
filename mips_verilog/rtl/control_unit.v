// =============================================================
// Control Unit — decodes opcode into pipeline control signals
// Matches the switch/case in your C++ decode() stage exactly
// =============================================================
module control_unit (
    input  wire [5:0] opcode,
    input  wire [5:0] funct,        // used for R-type ALUOp
    output reg        RegDst,
    output reg        ALUSrc,
    output reg        MemtoReg,
    output reg        RegWrite,
    output reg        MemRead,
    output reg        MemWrite,
    output reg        Branch,
    output reg [5:0]  ALUOp         // funct field for R-type, fixed code otherwise
);
    always @(*) begin
        // Safe defaults — bubble / unknown opcode
        {RegDst, ALUSrc, MemtoReg, RegWrite,
         MemRead, MemWrite, Branch} = 7'b0;
        ALUOp = 6'h00;

        case (opcode)
            6'h00: begin // R-type: add, sub, and, or, slt
                RegDst   = 1; RegWrite = 1;
                ALUOp    = funct;       // pass funct directly to ALU
            end
            6'h08: begin // addi
                ALUSrc   = 1; RegWrite = 1;
                ALUOp    = 6'h20;       // ADD
            end
            6'h23: begin // lw
                ALUSrc   = 1; MemtoReg = 1;
                RegWrite = 1; MemRead  = 1;
                ALUOp    = 6'h20;
            end
            6'h2B: begin // sw
                ALUSrc   = 1; MemWrite = 1;
                ALUOp    = 6'h20;
            end
            6'h04: begin // beq
                Branch   = 1;
                ALUOp    = 6'h22;       // SUB (check zero)
            end
        endcase
    end
endmodule