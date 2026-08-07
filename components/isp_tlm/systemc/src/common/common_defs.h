/*
 * Common Definitions for ISP TLM
 */

#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include <systemc>
#include <tlm>
#include <tlm_utils/peq_with_get.h>

// SystemC namespace
using namespace sc_core;
using namespace sc_dt;
using namespace tlm;

//=============================================================================
// Common Constants
//=============================================================================
constexpr unsigned int CYCLE = 10;  // 100MHz default clock

//=============================================================================
// TLM Generic Payload Extensions
//=============================================================================
struct isp_extension : tlm::tlm_extension<isp_extension> {
    unsigned int pixel_x;
    unsigned int pixel_y;
    bool frame_start;
    bool frame_end;

    isp_extension() : pixel_x(0), pixel_y(0), frame_start(false), frame_end(false) {}

    tlm_extension_base* clone() const override {
        isp_extension* ext = new isp_extension();
        ext->pixel_x = this->pixel_x;
        ext->pixel_y = this->pixel_y;
        ext->frame_start = this->frame_start;
        ext->frame_end = this->frame_end;
        return ext;
    }

    void copy_from(const tlm_extension_base& ext) override {
        const isp_extension* other = static_cast<const isp_extension*>(&ext);
        pixel_x = other->pixel_x;
        pixel_y = other->pixel_y;
        frame_start = other->frame_start;
        frame_end = other->frame_end;
    }
};

//=============================================================================
// TLM Phase Identifiers
//=============================================================================
enum isp_phase {
    BEGIN_PIXEL = 0,
    END_PIXEL = 1,
    BEGIN_LINE = 2,
    END_LINE = 3,
    BEGIN_FRAME = 4,
    END_FRAME = 5
};

//=============================================================================
// Logging Macros
//=============================================================================
#define ISP_LOG_INFO(msg) \
    SC_LOGGING_LOG_INFO_TYPE(this->name(), msg)

#define ISP_LOG_WARN(msg) \
    SC_LOGGING_LOG_WARN_TYPE(this->name(), msg)

#define ISP_LOG_ERROR(msg) \
    SC_LOGGING_LOG_ERROR_TYPE(this->name(), msg)

#define ISP_LOG_DEBUG(msg) \
    SC_LOGGING_LOG_DEBUG_TYPE(this->name(), msg)

//=============================================================================
// Assert Macros
//=============================================================================
#define ISP_ASSERT(condition, msg) \
    if (!(condition)) { \
        SC_REPORT_ERROR(SC_ID_ASSERTION_FAILED_, msg); \
    }

#endif // COMMON_DEFS_H
