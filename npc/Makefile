VERILOG_SRC ?= example.v
CPP_SRC ?= sim.cpp

all:
	@echo "Write this Makefile by your self."

sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@echo "Write this Makefile by your self."

	verilator --cc --exe --build -j 0 -Wall $(VERILOG_SRC) $(CPP_SRC)

include ../Makefile
