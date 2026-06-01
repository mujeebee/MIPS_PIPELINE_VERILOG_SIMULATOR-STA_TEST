// =============================================================
// 5-Stage Pipelined MIPS CPU
// Direct RTL translation of your C++ MIPSSimulator class.
//
// Mapping to C++ methods:
//   fetch()          → IF stage
//   decode()         → ID stage
//   execute()        → EX stage  (includes forwarding unit)
//   memory_access()  → MEM stage
//   write_back()     → WB stage
//
// Latch naming mirrors your structs:
//   IF_ID_Latch  → ifid_*
//   ID_EX_Latch  → idex_*
//   EX_MEM_Latch → exmem_*
//   MEM_WB_Latch → memwb_*
// =============================================================
`timescale 1ns/1ps

module mips_pipeline (
    input wire clk,
    input wire rst_n
);

// ---------------------------------------------------------------
// Instruction Memory (ROM)
// ---------------------------------------------------------------
reg [31:0] imem [0:255];
initial $readmemh("scripts/program.hex", imem, 0, 5); // 6 instructions

// ---------------------------------------------------------------
// Data Memory (matches your DataMemory[1024])
// ---------------------------------------------------------------
reg [31:0] dmem [0:255];   // 256 words = 1KB (word-addressed)

// ---------------------------------------------------------------
// Register File (32 × 32-bit)
// ---------------------------------------------------------------
reg [31:0] regfile [0:31];
integer i;
initial for (i = 0; i < 32; i = i + 1) regfile[i] = 32'd0;

// ==============================================================
// IF — Instruction Fetch
// ==============================================================
reg [31:0] PC;

// IF/ID pipeline register (= IF_ID_Latch in C++)
reg [31:0] ifid_PC4;
reg [31:0] ifid_instr;
reg        ifid_valid;

wire [31:0] IF_instr = imem[PC[9:2]];   // word-addressed fetch

// ==============================================================
// ID — Instruction Decode / Register Read
// ==============================================================
wire [5:0]  ID_opcode = ifid_instr[31:26];
wire [4:0]  ID_rs     = ifid_instr[25:21];
wire [4:0]  ID_rt     = ifid_instr[20:16];
wire [4:0]  ID_rd     = ifid_instr[15:11];
wire [5:0]  ID_funct  = ifid_instr[5:0];
wire [31:0] ID_simm   = {{16{ifid_instr[15]}}, ifid_instr[15:0]}; // sign-extend

// Control signals from control unit
wire        ID_RegDst, ID_ALUSrc, ID_MemtoReg, ID_RegWrite;
wire        ID_MemRead, ID_MemWrite, ID_Branch;
wire [5:0]  ID_ALUOp;

control_unit cu (
    .opcode   (ID_opcode),
    .funct    (ID_funct),
    .RegDst   (ID_RegDst),   .ALUSrc  (ID_ALUSrc),
    .MemtoReg (ID_MemtoReg), .RegWrite(ID_RegWrite),
    .MemRead  (ID_MemRead),  .MemWrite(ID_MemWrite),
    .Branch   (ID_Branch),   .ALUOp   (ID_ALUOp)
);

// Register file reads (with WB write-first forwarding)
// Mirrors your fix: if WB is writing the same reg, use WB data
wire [31:0] WB_data;   // declared below; Verilog allows forward reference in wire
wire [31:0] ID_rdata1 = (memwb_RegWrite && memwb_rd != 0 && memwb_rd == ID_rs)
                        ? WB_data : regfile[ID_rs];
wire [31:0] ID_rdata2 = (memwb_RegWrite && memwb_rd != 0 && memwb_rd == ID_rt)
                        ? WB_data : regfile[ID_rt];

// ID/EX pipeline register (= ID_EX_Latch in C++)
reg [31:0] idex_PC4;
reg [31:0] idex_rdata1, idex_rdata2;
reg [31:0] idex_simm;
reg [4:0]  idex_rs, idex_rt, idex_rd;
reg        idex_RegDst,  idex_ALUSrc,  idex_MemtoReg, idex_RegWrite;
reg        idex_MemRead, idex_MemWrite, idex_Branch;
reg [5:0]  idex_ALUOp;
reg        idex_valid;

// ==============================================================
// EX — Execute
// ==============================================================

// --- Forwarding unit ---
wire [1:0] fwd_a, fwd_b;

// EX/MEM and MEM/WB outputs (needed combinationally for forwarding)
// Declared as regs below; Verilog allows combinational reads before assignment
reg [4:0]  exmem_rd;
reg        exmem_RegWrite;
reg [4:0]  memwb_rd;
reg        memwb_RegWrite;
reg [31:0] exmem_alu_result;
reg [31:0] memwb_alu_result, memwb_read_data;
reg        memwb_MemtoReg;

forwarding_unit fwd_unit (
    .IDEX_rs        (idex_rs),
    .IDEX_rt        (idex_rt),
    .EXMEM_rd       (exmem_rd),
    .EXMEM_RegWrite (exmem_RegWrite),
    .MEMWB_rd       (memwb_rd),
    .MEMWB_RegWrite (memwb_RegWrite),
    .fwd_a          (fwd_a),
    .fwd_b          (fwd_b)
);

// WB data mux (used in both forwarding and write-back)
assign WB_data = memwb_MemtoReg ? memwb_read_data : memwb_alu_result;

// Forwarded operands
wire [31:0] EX_op1 = (fwd_a == 2'b10) ? exmem_alu_result :
                     (fwd_a == 2'b01) ? WB_data          : idex_rdata1;

wire [31:0] EX_op2_reg = (fwd_b == 2'b10) ? exmem_alu_result :
                         (fwd_b == 2'b01) ? WB_data          : idex_rdata2;

// ALUSrc mux — use immediate for LW/SW/ADDI, register for R-type/BEQ
wire [31:0] EX_alu_b = idex_ALUSrc ? idex_simm : EX_op2_reg;

// SW write_data: always the forwarded rt value (before ALUSrc mux)
wire [31:0] EX_st_wdata = EX_op2_reg;

// ALU
wire [31:0] EX_alu_result;
wire        EX_zero;

alu alu_inst (
    .a      (EX_op1),
    .b      (EX_alu_b),
    .op     (idex_ALUOp),
    .result (EX_alu_result),
    .zero   (EX_zero)
);

// BEQ: zero is op1 == op2 (the original register values, not alu_b)
wire EX_branch_taken = idex_Branch & (EX_op1 == EX_op2_reg);
wire [31:0] EX_branch_target = idex_PC4 + {idex_simm[29:0], 2'b00};

// Destination register mux (RegDst)
wire [4:0] EX_dest = idex_RegDst ? idex_rd : idex_rt;

// EX/MEM pipeline register (= EX_MEM_Latch in C++)
// (declared as regs above, assigned in always block below)
reg [31:0] exmem_write_data;
reg        exmem_MemtoReg, exmem_MemRead, exmem_MemWrite, exmem_Branch;
reg        exmem_valid;

// ==============================================================
// MEM — Memory Access
// ==============================================================
wire [31:0] MEM_read_data = exmem_MemRead ? dmem[exmem_alu_result[9:2]] : 32'd0;

// MEM/WB pipeline register (= MEM_WB_Latch in C++)
// (declared as regs above, assigned in always block below)
reg        memwb_RegWrite_r;   // alias — memwb_RegWrite already declared
reg        memwb_valid;

// ==============================================================
// WB — Write Back  (combinational; register file write in always)
// ==============================================================
// WB_data defined above (wire)

// ==============================================================
// Pipeline register updates + hazard logic
// ==============================================================
// flush: squash IF/ID and ID/EX when branch is taken (from EX stage)
wire flush = exmem_Branch && EX_zero;   // Note: checked before exmem latch latches

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        PC           <= 32'd0;
        ifid_valid   <= 0;   ifid_instr  <= 0;   ifid_PC4 <= 0;
        idex_valid   <= 0;
        exmem_valid  <= 0;
        memwb_valid  <= 0;
        exmem_RegWrite <= 0;
        memwb_RegWrite <= 0;
    end else begin

        // ===== WB: register file write =====
        if (memwb_valid && memwb_RegWrite && memwb_rd != 0)
            regfile[memwb_rd] <= WB_data;
        regfile[0] <= 32'd0;    // $zero always 0

        // ===== MEM/WB latch =====
        memwb_valid      <= exmem_valid;
        memwb_alu_result <= exmem_alu_result;
        memwb_read_data  <= MEM_read_data;
        memwb_rd         <= exmem_rd;
        memwb_RegWrite   <= exmem_valid ? exmem_RegWrite   : 1'b0;
        memwb_MemtoReg   <= exmem_MemtoReg;

        // Data memory write (SW)
        if (exmem_valid && exmem_MemWrite)
            dmem[exmem_alu_result[9:2]] <= exmem_write_data;

        // ===== EX/MEM latch =====
        if (idex_valid) begin
            exmem_valid      <= 1;
            exmem_alu_result <= EX_alu_result;
            exmem_write_data <= EX_st_wdata;
            exmem_rd         <= EX_dest;
            exmem_RegWrite   <= idex_RegWrite;
            exmem_MemtoReg   <= idex_MemtoReg;
            exmem_MemRead    <= idex_MemRead;
            exmem_MemWrite   <= idex_MemWrite;
            exmem_Branch     <= idex_Branch;
        end else begin
            exmem_valid    <= 0;
            exmem_RegWrite <= 0;
            exmem_MemWrite <= 0;
            exmem_MemRead  <= 0;
            exmem_Branch   <= 0;
        end

        // ===== Branch: redirect PC + flush IF/ID and ID/EX =====
        if (EX_branch_taken) begin
            PC         <= EX_branch_target;
            ifid_valid <= 0;  ifid_instr <= 0;
            idex_valid <= 0;
        end else begin
            // ===== IF/ID latch =====
            if (PC[9:2] < 256) begin
                ifid_instr <= IF_instr;
                ifid_PC4   <= PC + 4;
                ifid_valid <= 1;
                PC         <= PC + 4;
            end else begin
                ifid_instr <= 0;
                ifid_valid <= 0;
            end

            // ===== ID/EX latch =====
            if (ifid_valid && ifid_instr != 0) begin
                idex_valid   <= 1;
                idex_PC4     <= ifid_PC4;
                idex_rdata1  <= ID_rdata1;
                idex_rdata2  <= ID_rdata2;
                idex_simm    <= ID_simm;
                idex_rs      <= ID_rs;
                idex_rt      <= ID_rt;
                idex_rd      <= ID_rd;
                idex_RegDst  <= ID_RegDst;
                idex_ALUSrc  <= ID_ALUSrc;
                idex_MemtoReg<= ID_MemtoReg;
                idex_RegWrite<= ID_RegWrite;
                idex_MemRead <= ID_MemRead;
                idex_MemWrite<= ID_MemWrite;
                idex_Branch  <= ID_Branch;
                idex_ALUOp   <= ID_ALUOp;
            end else begin
                idex_valid    <= 0;
                idex_RegWrite <= 0;
                idex_MemRead  <= 0;
                idex_MemWrite <= 0;
                idex_Branch   <= 0;
            end
        end

    end
end

endmodule