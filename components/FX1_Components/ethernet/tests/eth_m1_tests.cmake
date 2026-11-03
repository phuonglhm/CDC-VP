# Registers the M1 targeted regressions of test_eth_mac_m1, one CTest per mode.
# Included by tests/CMakeLists.txt, and by the review harness that builds the
# same test against the unmodified baseline (fail-before evidence).
set(_eth_m1_modes rx-fifo-reset unicast-absent param-reset csr-contract collisions coal-collision dma-bus)
foreach(_m IN LISTS _eth_m1_modes)
    add_test(NAME eth_m1_${_m} COMMAND test_eth_mac_m1 ${_m})
    set_tests_properties(eth_m1_${_m} PROPERTIES LABELS "unit;m1" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "=== eth_m1 ${_m}: [0-9]+ checks, 0 failures ===")
endforeach()

# invalid:<case> must fail fast, naming the offending parameter: <case>=<word in the message>
set(_eth_m1_invalid
    axi_data_bytes=axi_data_bytes axi_max_burst_0=axi_max_burst axi_max_burst_17=axi_max_burst
    rx_unicast_num_0=rx_unicast_num rx_unicast_num_5=rx_unicast_num mdio_clk_div=mdio_clk_div
    poll_cycles=poll_cycles fifo_pow2=rx_fifo_bytes fifo_capacity=tx_fifo_bytes
    tx_meta_depth=tx_meta_depth dram_base=dram_base clk_period=clk_period csr_window=csr_window
    max_frame_len=max_frame_len)
foreach(_pair IN LISTS _eth_m1_invalid)
    string(REPLACE "=" ";" _kv "${_pair}")
    list(GET _kv 0 _case)
    list(GET _kv 1 _word)
    add_test(NAME eth_m1_invalid_${_case} COMMAND test_eth_mac_m1 invalid:${_case})
    set_tests_properties(eth_m1_invalid_${_case} PROPERTIES LABELS "unit;m1" TIMEOUT 30
        PASS_REGULAR_EXPRESSION "FAILFAST invalid:${_case}: .*${_word}" FAIL_REGULAR_EXPRESSION "ACCEPTED")
endforeach()

# Same-edge rules of CsrFile / core::Rmon on their own (needs the M1 API, so
# the v1.0.6 fail-before harness leaves the target out).
if(TARGET test_eth_m1_units)
    foreach(_m csr-edge rmon-wrap)
        add_test(NAME eth_m1_${_m} COMMAND test_eth_m1_units ${_m})
        set_tests_properties(eth_m1_${_m} PROPERTIES LABELS "unit;m1" TIMEOUT 120
            PASS_REGULAR_EXPRESSION "=== eth_m1 ${_m}: [0-9]+ checks, 0 failures ===")
    endforeach()
endif()
