#pragma once
#include <h264/intra/intra_core.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Test-only H.264 sample oracle: explicit 4x4 coefficient matrices and
// dot-product plane gradients. No call to production prediction helpers.
namespace intra_test {
inline std::vector<uint8_t> prediction(h264::intra::Kind kind,unsigned mode,
                                       const h264::intra::References& r) {
    using h264::intra::Kind;
    const unsigned n=kind==Kind::Luma4x4 ? 4:kind==Kind::Luma16x16 ? 16:8;
    std::vector<uint8_t> out(n*n);
    const unsigned dc_mode=kind==Kind::Chroma8x8 ? 0:2;
    const unsigned vertical=kind==Kind::Chroma8x8 ? 2:0;
    if(mode==vertical || mode==1) {
        for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x)
            out[y*n+x]=mode==vertical ? r.top[x]:r.left[y];
        return out;
    }
    if(mode==dc_mode) {
        for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x) {
            unsigned sum=0,count=0;
            bool top=r.has_top,left=r.has_left;
            unsigned first_x=0,first_y=0,length=n;
            if(kind==Kind::Chroma8x8) {
                first_x=x<4 ? 0:4; first_y=y<4 ? 0:4; length=4;
                if(top && left && x>=4 && y<4) left=false;
                if(top && left && x<4 && y>=4) top=false;
            }
            for(unsigned i=0;i<length;++i) {
                if(top) { sum+=r.top[first_x+i]; ++count; }
                if(left) { sum+=r.left[first_y+i]; ++count; }
            }
            out[y*n+x]=uint8_t(count ? (sum+count/2)/count:128);
        }
        return out;
    }
    if(kind!=Kind::Luma4x4) {
        const int weights16[]={-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8};
        const int weights8[]={-3,-2,-1,0,1,2,3,4};
        const int* weights=n==16 ? weights16:weights8;
        int h=-int(n/2)*r.upper_left,v=h;
        for(unsigned i=0;i<n;++i) { h+=weights[i]*r.top[i]; v+=weights[i]*r.left[i]; }
        const int b=int(std::floor(double((n==16 ? 5:17)*h+(n==16 ? 32:16))/(n==16 ? 64:32)));
        const int c=int(std::floor(double((n==16 ? 5:17)*v+(n==16 ? 32:16))/(n==16 ? 64:32)));
        const int a=16*(int(r.top[n-1])+r.left[n-1]);
        for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x) {
            const int value=int(std::floor(double(a+b*(int(x)-int(n/2)+1)+c*(int(y)-int(n/2)+1)+16)/32));
            out[y*n+x]=uint8_t(std::clamp(value,0,255));
        }
        return out;
    }
    int e[13];
    for(unsigned i=0;i<8;++i) e[i]=i>=4&&!r.has_upper_right ? r.top[3]:r.top[i];
    for(unsigned i=0;i<4;++i) e[8+i]=r.left[i];
    e[12]=r.upper_left;
    const auto half=[&](unsigned a,unsigned b){ return uint8_t((e[a]+e[b]+1)/2); };
    const auto mix=[&](unsigned a,unsigned b,unsigned c){ return uint8_t((e[a]+2*e[b]+e[c]+2)/4); };
    switch(mode) {
    case 3: return {
        mix(0,1,2),mix(1,2,3),mix(2,3,4),mix(3,4,5),
        mix(1,2,3),mix(2,3,4),mix(3,4,5),mix(4,5,6),
        mix(2,3,4),mix(3,4,5),mix(4,5,6),mix(5,6,7),
        mix(3,4,5),mix(4,5,6),mix(5,6,7),mix(6,7,7)};
    case 4: return {
        mix(8,12,0),mix(12,0,1),mix(0,1,2),mix(1,2,3),
        mix(9,8,12),mix(8,12,0),mix(12,0,1),mix(0,1,2),
        mix(10,9,8),mix(9,8,12),mix(8,12,0),mix(12,0,1),
        mix(11,10,9),mix(10,9,8),mix(9,8,12),mix(8,12,0)};
    case 5: return {
        half(12,0),half(0,1),half(1,2),half(2,3),
        mix(8,12,0),mix(12,0,1),mix(0,1,2),mix(1,2,3),
        mix(12,8,9),half(12,0),half(0,1),half(1,2),
        mix(8,9,10),mix(8,12,0),mix(12,0,1),mix(0,1,2)};
    case 6: return {
        half(12,8),mix(8,12,0),mix(12,0,1),mix(0,1,2),
        half(8,9),mix(12,8,9),half(12,8),mix(8,12,0),
        half(9,10),mix(8,9,10),half(8,9),mix(12,8,9),
        half(10,11),mix(9,10,11),half(9,10),mix(8,9,10)};
    case 7: return {
        half(0,1),half(1,2),half(2,3),half(3,4),
        mix(0,1,2),mix(1,2,3),mix(2,3,4),mix(3,4,5),
        half(1,2),half(2,3),half(3,4),half(4,5),
        mix(1,2,3),mix(2,3,4),mix(3,4,5),mix(4,5,6)};
    case 8: return {
        half(8,9),mix(8,9,10),half(9,10),mix(9,10,11),
        half(9,10),mix(9,10,11),half(10,11),mix(10,11,11),
        half(10,11),mix(10,11,11),uint8_t(e[11]),uint8_t(e[11]),
        uint8_t(e[11]),uint8_t(e[11]),uint8_t(e[11]),uint8_t(e[11])};
    default: throw std::invalid_argument("oracle unsupported mode");
    }
}
}
