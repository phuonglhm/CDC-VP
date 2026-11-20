#!/bin/bash
# ModelSim run of the opgen bench: bash run_ms.sh <rtl dma dir> <case dir> [group ...]
# One group = one directory with cmds.txt + mem_init.hex (made by opgen_cases.py gen); sim.log + dump.hex are written there.
# vlib / vlog / vsim must be on PATH, or MODELSIM_BIN must name the directory that holds them.
set -u
DMA=$1; CASES=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
if [ -n "${MODELSIM_BIN:-}" ]; then export PATH=$MODELSIM_BIN:$PATH; fi
B=$CASES/_build
if [ ! -d $B/work ]; then
  mkdir -p $B && cd $B && vlib work >/dev/null && \
  vlog -sv -quiet $DMA/redma_pkg.sv $DMA/ar_engine.sv $DMA/aw_engine.sv $DMA/b_engine.sv $DMA/conf_regs.sv $DMA/data_fifo.sv $DMA/r_engine.sv \
       $DMA/realigner.sv $DMA/redma_lite_demux.sv $DMA/redma_opgen.sv $DMA/redma_opgen_top.sv $DMA/redma_seg_glue.sv $DMA/redma_top.sv \
       $DMA/transaction_generator.sv $DMA/w_engine.sv $HERE/axi_mem_ideal.sv $HERE/tb_opgen.sv 2>&1 | tail -20 || exit 1
  cd - >/dev/null
fi
GROUPS_=${@:-$(ls $CASES | grep -v _build)}
for g in $GROUPS_; do
  d=$CASES/$g
  ( cd $d && cp -r $B/work ./work_tmp 2>/dev/null; rm -rf work; mv work_tmp work
    vsim -c -quiet -suppress 3009,3017,12110 work.tb_opgen -do "run -all; quit -f" > sim.log 2>&1
    rm -rf work
    echo "$g: $(grep -c '^SEG ' sim.log) segments, $(grep -E '^(END|TIMEOUT)' sim.log | tail -1)" )
done
