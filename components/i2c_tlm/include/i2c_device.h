#pragma once
// Byte-level I2C bus device, attached to the i2c controller model in
// functional host mode (i2c::attach). One call per bus event, in bus order;
// the controller spends the bit times around them.
#include <cstdint>

class i2c_device {
public:
    virtual ~i2c_device() = default;
    // START or repeated START followed by this device's address. read: R/W
    // bit of the address byte. Returns the ACK of the address byte.
    virtual bool i2c_start(bool read) = 0;
    // One data byte written by the controller; returns its ACK.
    virtual bool i2c_write(uint8_t byte) = 0;
    // One data byte read by the controller; ack: the controller acknowledges
    // it (more bytes follow) or not (last byte of the read).
    virtual uint8_t i2c_read(bool ack) = 0;
    // STOP condition ending the transaction the device took part in.
    virtual void i2c_stop() = 0;
};
