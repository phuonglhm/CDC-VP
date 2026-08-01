// Software IEEE-754 half-precision storage type, extracted from sauria_types.h so
// the driver libraries (libsauria_mem) can use it WITHOUT pulling in SystemC.
// g++ 11.4 in C++17 mode does not expose _Float16, so conversion is done by hand
// (RNE rounding). Verified bit-exact against numpy.float16 over all 65536 half
// values (decode) and 300k+ random/edge float32 values (encode).
//
// Used as T_ACT/T_WEI only; T_PSUM stays plain float (accumulate wide, round once
// at DRAM write-out) to match the SAURIA "ideal" reference model.

#ifndef SAURIA_FP16_H
#define SAURIA_FP16_H

#include <cstdint>
#include <cstring>
#include <iostream>

namespace sauria
{
    struct fp16_t
    {
        uint16_t bits{0};

        fp16_t() = default;
        fp16_t(float f) { bits = float_to_half(f); }

        operator float() const { return half_to_float(bits); }
        fp16_t &operator=(float f)
        {
            bits = float_to_half(f);
            return *this;
        }

        bool operator==(const fp16_t &other) const { return bits == other.bits; }

        friend std::ostream &operator<<(std::ostream &os, const fp16_t &v)
        {
            os << static_cast<float>(v);
            return os;
        }

        static uint16_t float_to_half(float f)
        {
            uint32_t x;
            std::memcpy(&x, &f, sizeof(x));
            uint32_t sign = (x >> 16) & 0x8000u;
            int32_t exp = static_cast<int32_t>((x >> 23) & 0xFFu) - 127 + 15;
            uint32_t mant = x & 0x7FFFFFu;

            if (((x >> 23) & 0xFFu) == 0xFFu)
            {
                // Inf / NaN
                return static_cast<uint16_t>(sign | 0x7C00u | (mant ? 0x200u : 0u));
            }
            if (exp <= 0)
            {
                if (exp < -10)
                    return static_cast<uint16_t>(sign); // underflow to zero
                mant |= 0x800000u;                       // implicit leading 1
                int shift = 14 - exp;                     // shift for denormal
                uint32_t half_mant = mant >> shift;
                uint32_t remainder = mant & ((1u << shift) - 1u);
                uint32_t halfway = 1u << (shift - 1);
                if (remainder > halfway || (remainder == halfway && (half_mant & 1u)))
                    half_mant++;
                return static_cast<uint16_t>(sign | half_mant);
            }
            if (exp >= 0x1F)
            {
                return static_cast<uint16_t>(sign | 0x7C00u); // overflow -> inf
            }
            uint32_t half_mant = mant >> 13;
            uint32_t remainder = mant & 0x1FFFu;
            if (remainder > 0x1000u || (remainder == 0x1000u && (half_mant & 1u)))
            {
                half_mant++;
                if (half_mant == 0x400u)
                {
                    half_mant = 0;
                    exp++;
                    if (exp >= 0x1F)
                        return static_cast<uint16_t>(sign | 0x7C00u);
                }
            }
            return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | half_mant);
        }

        static float half_to_float(uint16_t h)
        {
            uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
            uint32_t exp = (h >> 10) & 0x1Fu;
            uint32_t mant = h & 0x3FFu;
            uint32_t x;
            if (exp == 0)
            {
                if (mant == 0)
                {
                    x = sign;
                }
                else
                {
                    int e = -1;
                    do
                    {
                        mant <<= 1;
                        e++;
                    } while (!(mant & 0x400u));
                    mant &= 0x3FFu;
                    uint32_t fexp = static_cast<uint32_t>(127 - 15 - e);
                    x = sign | (fexp << 23) | (mant << 13);
                }
            }
            else if (exp == 0x1F)
            {
                x = sign | 0x7F800000u | (mant << 13);
            }
            else
            {
                uint32_t fexp = exp - 15 + 127;
                x = sign | (fexp << 23) | (mant << 13);
            }
            float f;
            std::memcpy(&f, &x, sizeof(f));
            return f;
        }
    };

} // namespace sauria

#endif // SAURIA_FP16_H
