# FX1 components, configured from the CDC-VP root when CDC_BUILD_FX1_SOC=ON.
# The bus also builds standalone (see its README); the others need the
# CDC-VP root for SystemC::systemc.

# Memory map / interrupt map shared by the platform (C++) and firmware (C).
add_subdirectory(fx1_map)

add_subdirectory(bus)
add_subdirectory(sparse_ram)
add_subdirectory(access_guard)
add_subdirectory(clint)
add_subdirectory(plic)
add_subdirectory(cpu_port)
add_subdirectory(exclusive_monitor)
add_subdirectory(sim_control)
add_subdirectory(dma)
add_subdirectory(isp)
add_subdirectory(ddr3)

# MIPI CSI-2 RX (detailed D-PHY model + BFM). Off by default: the D-PHY
# netlist is ~180 large translation units. Even when ON it is EXCLUDE_FROM_ALL,
# so neither `all` nor fx1_all builds it; build `fx1_mipi_csi2_all` explicitly.
option(FX1_BUILD_MIPI_CSI2 "Configure the MIPI CSI-2 RX component (D-PHY model, BFM, tests)" OFF)
if(FX1_BUILD_MIPI_CSI2)
  add_subdirectory(mipi-csi2 EXCLUDE_FROM_ALL)
endif()
