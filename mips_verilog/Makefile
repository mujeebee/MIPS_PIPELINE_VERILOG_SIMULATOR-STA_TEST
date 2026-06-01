## Makefile — MIPS Pipeline (Verilog)
SIM   = iverilog
RUN   = vvp
SRCS  = rtl/control_unit.v rtl/alu_fwd.v rtl/mips_pipeline.v tb/tb_mips.v
OUT   = sim_out

.PHONY: all sim wave clean

all: sim

sim:
	$(SIM) -g2012 -o $(OUT) $(SRCS)
	$(RUN) $(OUT)

wave: sim
	gtkwave mips_pipeline.vcd &

clean:
	rm -f $(OUT) mips_pipeline.vcd