/* SPDX-License-Identifier: Apache-2.0
 *
 * Interactive FreeRTOS console for VP_FX1_Full_SoC.
 */
#ifndef FREERTOS_FX1_CLI_H
#define FREERTOS_FX1_CLI_H

/* Print the banner and process commands forever. UART RX must already have
 * been enabled with uart_rx_start(). */
void cli_run(void);

#endif /* FREERTOS_FX1_CLI_H */
