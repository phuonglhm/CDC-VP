/* SPDX-License-Identifier: Apache-2.0
 *
 * Out-of-band UART control marker for the host-assisted NoC dashboard.
 *
 * The command still enters firmware through UART0 RX and PLIC source 1. After
 * the firmware CLI accepts it, this marker leaves through the normal UART0 TX
 * path. noc_soc consumes it as a control record, atomically publishes the live
 * metrics JSON, and the host client renders that JSON with noc_dashboard.py.
 */
#ifndef CDC_NOC_DASHBOARD_PROTOCOL_H
#define CDC_NOC_DASHBOARD_PROTOCOL_H

#define CDC_NOC_DASHBOARD_REQUEST_BODY "FLOONOC_DASHBOARD_V1"
#define CDC_NOC_DASHBOARD_REQUEST \
    "\x1e" CDC_NOC_DASHBOARD_REQUEST_BODY "\x1f\n"

#endif /* CDC_NOC_DASHBOARD_PROTOCOL_H */
