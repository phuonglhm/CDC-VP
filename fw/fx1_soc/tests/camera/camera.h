/*
 * CSI-2 M5b camera smokes on fx1_soc --camera: shared bring-up, interrupt
 * handling and frame checks, on the BSP drivers (fw/fx1_soc/drivers) and the
 * reference CSI-2 RX driver.
 *
 * Path under test: FW -> I2C0 -> CCS sensor model -> D-PHY TX BFM -> detailed
 * D-PHY RX -> CSI-2 RX -> MIPI_DMA -> DDR, csi2_irq -> PLIC source 7 -> trap.
 * The interrupt handler only claims, collects the controller's pending
 * events (fx1_csi2_irq_handle) and completes; the test body acts on them.
 */
#ifndef FX1_CAMERA_TEST_H
#define FX1_CAMERA_TEST_H

#include <stdint.h>

#include "fx1_ccs_sensor.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_fw.h"
#include "fx1_i2c.h"

#define CAM_W 64u
#define CAM_H 16u
#define CAM_LINE_LENGTH 2000u           /* 10 us lines at the sensor's 200 MHz pixel clock */
/* 3 ms frame period: 160 us of lines, then blanking long enough for the
 * firmware to check a frame pixel by pixel and refill its side (~1.5 ms) before
 * the next one, so the smokes see no drop they did not ask for, and only the
 * frames they need cross the (slow) detailed PHY. */
#define CAM_FRAME_LENGTH 300u
#define CAM_FRAME_US 3000u              /* frame period */
#define CAM_WAIT_US (3u * CAM_FRAME_US) /* budget for one frame event */
#define CAM_SIDE0 FX1_FW_RAW_BUF_BASE
#define CAM_SIDE1 (FX1_FW_RAW_BUF_BASE + 0x10000u)
#define CAM_SIDE_SIZE 0x1000u
#define CAM_FILL 0xA5u                  /* buffer contents before the DMA */

/* IRQ_STATUS bit numbers (csi2_rx_controller.h IrqBit). */
#define CAM_IRQ_FRAME_DROPPED 14u
#define CAM_IRQ_BUF_DONE_OK 16u
#define CAM_IRQ_BUF_DONE_ERR 17u
#define CAM_IRQ_AXI_WRITE_ERROR 18u
#define CAM_IRQ_RESOLUTION_MISMATCH 20u
#define CAM_BIT(b) (1u << (b))

typedef struct cam {
    fx1_i2c_t bus;
    fx1_ccs sensor;
    fx1_csi2_dev dev;
    fx1_ccs_mode mode;
    uint32_t base[2];
    uint32_t bpl;      /* bytes per line in DDR (stride, no padding) */
    int raw16;         /* DMA layout: 1 RAW16 right-aligned, 0 packed */
} cam_t;

extern cam_t g_cam;
/* Events collected by the handler (IRQ_STATUS bits), cleared by cam_take(). */
extern volatile uint32_t g_cam_events;
extern volatile uint32_t g_cam_irqs;
/* Simulated time cam_expect_frame spent on its last frame after the event
 * (number, check, refill, release), in us. */
extern uint32_t g_cam_handle_us;
/* mtime when cam_expect_frame saw its last frame's event (about its FE). */
extern uint64_t g_cam_event_mtime;

/* I2C0 open (polling), sensor probe, CSI-2 probe, PHY start, DMA programmed
 * and started with sides base0/base1, interrupts enabled at the controller
 * and the PLIC (source 7, hart 0), trap handler installed, MIE on. Buffers
 * filled with CAM_FILL. The sensor mode is programmed (standby). */
void cam_bringup(uint32_t bits, uint32_t lanes, int raw16, uint32_t base0, uint32_t base1);
/* Programs side bases / layout again (DMA stopped and idle), restarts it. */
void cam_dma_restart(uint32_t base0, uint32_t base1);

/* Waits (polling, no DDR writes) until an event in `mask` was collected or
 * `timeout_us` passed; returns the events collected so far and clears them. */
uint32_t cam_wait(uint32_t mask, uint32_t timeout_us);
/* Side holding a completed frame in state `state`, or -1. */
int cam_side_in(fx1_csi2_buf_state state);
/* Frame number of the image in side `side`, from its first pixel (the pattern
 * is (x*37 + y*101 + f*53 + 5) mod 2^bits; 53 is odd, so f is unique below
 * 2^bits), 0 if no number matches. */
uint32_t cam_frame_number(int side);
/* Every pixel of side `side` equals the pattern of frame f, and the bytes
 * after the frame still hold CAM_FILL. Logs the first mismatch. */
int cam_check_frame(int side, uint32_t f);
/* Releases the side and refills it with CAM_FILL first. */
void cam_release(int side);
/* Expects one valid frame numbered f: waits for BUF_DONE_OK, checks, releases. */
void cam_expect_frame(uint32_t f, uint32_t code);
/* Stream off, then lets the frame in progress end and handles it (valid,
 * checked against its number, released); returns frames handled. */
uint32_t cam_stream_off_and_drain(uint32_t code);
/* Busy-waits `us` of simulated time. */
void cam_delay_us(uint32_t us);

#endif /* FX1_CAMERA_TEST_H */
