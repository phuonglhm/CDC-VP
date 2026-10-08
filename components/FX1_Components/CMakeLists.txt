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
