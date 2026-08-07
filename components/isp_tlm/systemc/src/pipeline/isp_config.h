/*
 * ISP Configuration Management
 * Handles runtime configuration of ISP parameters
 */

#ifndef ISP_CONFIG_H
#define ISP_CONFIG_H

#include "common/isp_types.h"
#include "common/isp_params.h"
#include <map>
#include <string>

//=============================================================================
// ISP Configuration Registry
// Centralized configuration management
//=============================================================================
class isp_config {
public:
    // Singleton access
    static isp_config& get_instance() {
        static isp_config instance;
        return instance;
    }

    //=============================================================================
    // Image Configuration
    //=============================================================================
    void set_sensor_size(unsigned width, unsigned height) {
        m_sns_width = width;
        m_sns_height = height;
    }

    void set_crop_size(unsigned width, unsigned height) {
        m_crop_width = width;
        m_crop_height = height;
    }

    void set_bayer_pattern(BayerPattern pattern) {
        m_bayer = pattern;
    }

    //=============================================================================
    // Block Enable/Disable
    //=============================================================================
    void set_enable(const std::string& block, bool enable) {
        m_enables[block] = enable;
    }

    bool get_enable(const std::string& block) const {
        auto it = m_enables.find(block);
        return (it != m_enables.end()) ? it->second : false;
    }

    //=============================================================================
    // BLC Configuration
    //=============================================================================
    void set_blc(const BlcConfig& cfg) { m_blc = cfg; }
    const BlcConfig& get_blc() const { return m_blc; }

    //=============================================================================
    // WB Configuration
    //=============================================================================
    void set_wb(const WbConfig& cfg) { m_wb = cfg; }
    const WbConfig& get_wb() const { return m_wb; }

    //=============================================================================
    // CCM Configuration
    //=============================================================================
    void set_ccm(const CcmConfig& cfg) { m_ccm = cfg; }
    const CcmConfig& get_ccm() const { return m_ccm; }

    //=============================================================================
    // BNR Configuration
    //=============================================================================
    void set_bnr(const BnrConfig& cfg) { m_bnr = cfg; }
    const BnrConfig& get_bnr() const { return m_bnr; }

    //=============================================================================
    // Sharpening Configuration
    //=============================================================================
    void set_sharpen(const SharpenConfig& cfg) { m_sharpen = cfg; }
    const SharpenConfig& get_sharpen() const { return m_sharpen; }

    //=============================================================================
    // 2DNR Configuration
    //=============================================================================
    void set_nr2d(const Nr2dConfig& cfg) { m_nr2d = cfg; }
    const Nr2dConfig& get_nr2d() const { return m_nr2d; }

    //=============================================================================
    // Getters
    //=============================================================================
    unsigned get_sns_width() const { return m_sns_width; }
    unsigned get_sns_height() const { return m_sns_height; }
    unsigned get_crop_width() const { return m_crop_width; }
    unsigned get_crop_height() const { return m_crop_height; }
    BayerPattern get_bayer() const { return m_bayer; }

    //=============================================================================
    // Default configuration
    //=============================================================================
    void load_defaults() {
        m_sns_width = isp_params::DEFAULT_SNS_WIDTH;
        m_sns_height = isp_params::DEFAULT_SNS_HEIGHT;
        m_crop_width = isp_params::DEFAULT_CROP_WIDTH;
        m_crop_height = isp_params::DEFAULT_CROP_HEIGHT;
        m_bayer = isp_params::DEFAULT_BAYER;

        // Default enables
        IspEnables defaults;
        m_enables["crop"] = defaults.crop_en;
        m_enables["dpc"] = defaults.dpc_en;
        m_enables["blc"] = defaults.blc_en;
        m_enables["oecf"] = defaults.oecf_en;
        m_enables["dgain"] = defaults.dgain_en;
        m_enables["lsc"] = defaults.lsc_en;
        m_enables["bnr"] = defaults.bnr_en;
        m_enables["wb"] = defaults.wb_en;
        m_enables["demosaic"] = defaults.demosic_en;
        m_enables["ccm"] = defaults.ccm_en;
        m_enables["gamma"] = defaults.gamma_en;
        m_enables["csc"] = defaults.csc_en;
        m_enables["sharpen"] = defaults.sharpen_en;
        m_enables["ldci"] = defaults.ldci_en;
        m_enables["nr2d"] = defaults.nr2d_en;

        // Default BLC
        m_blc = {0, 0, 0, 0, false, 4096, 4096, 4096, 4096};

        // Default WB
        m_wb = {512, 512};  // Neutral gains

        // Default CCM (identity matrix)
        m_ccm = {
            512, 0, 0,    // R row
            0, 512, 0,    // G row
            0, 0, 512     // B row
        };
    }

    //=============================================================================
    // Load from file (optional)
    //=============================================================================
    void load_from_file(const std::string& filename) {
        // TODO: Implement file loading (JSON/YAML)
    }

    void save_to_file(const std::string& filename) {
        // TODO: Implement file saving
    }

private:
    isp_config() {
        load_defaults();
    }

    isp_config(const isp_config&) = delete;
    isp_config& operator=(const isp_config&) = delete;

    // Image dimensions
    unsigned m_sns_width;
    unsigned m_sns_height;
    unsigned m_crop_width;
    unsigned m_crop_height;
    BayerPattern m_bayer;

    // Block enables
    std::map<std::string, bool> m_enables;

    // Block configurations
    BlcConfig m_blc;
    WbConfig m_wb;
    CcmConfig m_ccm;
    BnrConfig m_bnr;
    SharpenConfig m_sharpen;
    Nr2dConfig m_nr2d;
};

#endif // ISP_CONFIG_H
