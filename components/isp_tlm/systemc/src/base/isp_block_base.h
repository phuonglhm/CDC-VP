/*
 * ISP Block Base Class
 * Provides common functionality for all ISP blocks
 */

#ifndef ISP_BLOCK_BASE_H
#define ISP_BLOCK_BASE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/peq_with_get.h>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "common/isp_params.h"

//=============================================================================
// Base class for all ISP processing blocks
//=============================================================================
template<unsigned int BITS>
class isp_block_base : public sc_module {
public:
    // Clock and reset
    sc_in<bool> pclk;
    sc_in<bool> rst_n;

    // Module enable
    sc_in<bool> enable{"enable"};  // Module-specific enable

    // Constructor
    isp_block_base(const sc_module_name& name)
        : sc_module(name)
        , pclk("pclk")
        , rst_n("rst_n")
        , m_enabled(false)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
    {
        // Register process
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();
    }

    virtual ~isp_block_base() = default;

    //=============================================================================
    // Pure virtual methods - must be implemented by derived classes
    //=============================================================================
    virtual void process_pixel(uint16_t in_pixel, uint16_t& out_pixel) = 0;
    virtual int get_pipeline_delay() const = 0;

    //=============================================================================
    // Virtual methods with default implementations
    //=============================================================================

    // Called at frame start
    virtual void reset_frame() {
        m_pixel_count = 0;
        m_line_count = 0;
    }

    // Called at line start
    virtual void reset_line() {
        m_pixel_count = 0;
    }

    // Get current position
    void get_position(unsigned& x, unsigned& y) const {
        x = m_pixel_count;
        y = m_line_count;
    }

    // Check if module is enabled
    bool is_enabled() const { return m_enabled; }

    // Set enable state
    void set_enabled(bool en) { m_enabled = en; }

    // Get frame count
    unsigned int get_frame_count() const { return m_frame_count; }

protected:
    //=============================================================================
    // Derived classes can use these utilities
    //=============================================================================

    // Extract pixel from bayer pattern
    static BayerPattern get_bayer_at(unsigned x, unsigned y, BayerPattern bayer) {
        int shift = ((y & 1) << 1) | (x & 1);
        switch (bayer) {
            case BayerPattern::RGGB: return BayerPattern::RGGB;  // Direct mapping
            case BayerPattern::GRBG: return (shift == 0 || shift == 3) ? BayerPattern::GRBG : BayerPattern::GBRG;
            case BayerPattern::GBRG: return (shift == 0 || shift == 3) ? BayerPattern::GBRG : BayerPattern::GRBG;
            case BayerPattern::BGGR: return BayerPattern::BGGR;
            default: return BayerPattern::RGGB;
        }
    }

    // Get channel from bayer position
    enum class Channel { R, G, B };

    static Channel get_channel_at(unsigned x, unsigned y, BayerPattern bayer) {
        int pattern_x = x & 1;
        int pattern_y = y & 1;

        switch (bayer) {
            case BayerPattern::RGGB:
                return (pattern_y == 0) ? ((pattern_x == 0) ? Channel::R : Channel::G)
                                       : ((pattern_x == 0) ? Channel::G : Channel::B);
            case BayerPattern::GRBG:
                return (pattern_y == 0) ? ((pattern_x == 0) ? Channel::G : Channel::R)
                                       : ((pattern_x == 0) ? Channel::G : Channel::B);
            case BayerPattern::GBRG:
                return (pattern_y == 0) ? ((pattern_x == 0) ? Channel::G : Channel::B)
                                       : ((pattern_x == 0) ? Channel::R : Channel::G);
            case BayerPattern::BGGR:
                return (pattern_y == 0) ? ((pattern_x == 0) ? Channel::B : Channel::G)
                                       : ((pattern_x == 0) ? Channel::G : Channel::R);
            default:
                return Channel::G;
        }
    }

    // Clamp to output bit range
    uint16_t clamp_output(uint32_t val) const {
        constexpr uint16_t MAX = (1 << BITS) - 1;
        return (val > MAX) ? MAX : static_cast<uint16_t>(val);
    }

    //=============================================================================
    // Member variables
    //=============================================================================
    bool m_enabled;
    unsigned m_pixel_count;
    unsigned m_line_count;
    unsigned m_frame_count;

private:
    //=============================================================================
    // Internal processes
    //=============================================================================

    void process_thread() {
        while (true) {
            wait();

            if (m_enabled) {
                // Derived classes handle actual processing
                // through their overridden process_pixel method
            }
        }
    }

    void reset_handler() {
        wait();
        reset_frame();
        m_frame_count = 0;
    }
};

//=============================================================================
// Typedef for common instantiations
//=============================================================================
using isp_block_base_10b = isp_block_base<10>;
using isp_block_base_8b = isp_block_base<8>;

#endif // ISP_BLOCK_BASE_H
