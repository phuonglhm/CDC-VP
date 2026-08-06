/* SPDX-License-Identifier: Apache-2.0 */

#ifndef FREERTOS_NOC_SOC_CLI_H
#define FREERTOS_NOC_SOC_CLI_H

/* CLI task entry. The task arms UART0 RX immediately, then waits until the
 * Step 12.4 supervisor releases it after the complete bring-up has passed. */
void noc_cli_task(void *params);

#endif /* FREERTOS_NOC_SOC_CLI_H */
