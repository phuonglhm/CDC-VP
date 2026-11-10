/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "isp_auto_policy.h"
static void add(isp_auto_meter *m,unsigned r,unsigned g,unsigned b,unsigned count,unsigned n)
{
    uint64_t sums[3]={(uint64_t)r*count,(uint64_t)g*count,(uint64_t)b*count};
    for (unsigned i=0;i<n;++i) isp_auto_add(m,sums,count);
}
static isp_auto_choice decide(isp_auto_meter *m)
{ return isp_auto_decide(m,499,256,422,160,576,16384); }
int main(void)
{
    isp_auto_meter m={0};isp_auto_choice c=decide(&m);
    assert(!c.valid && c.r==499 && c.b==422 && c.key==160);
    add(&m,0,0,0,1024,512);assert(!decide(&m).valid);
    add(&m,4095,4095,4095,1024,512);assert(!decide(&m).valid);
    add(&m,2000,500,200,1024,512);assert(!decide(&m).valid);
    add(&m,1000,1000,700,1,512);assert(!decide(&m).valid);
    add(&m,1000,1000,700,1024,7);assert(!decide(&m).valid);
    add(&m,1000,1000,700,1024,1);c=decide(&m);
    assert(c.valid && c.b>422 && c.r==499 && c.blend==256);
    assert(c.key==176 && c.white==384 && c.beta==24576);
    m=(isp_auto_meter){0};add(&m,1000,1000,1000,1024,16);c=decide(&m);
    assert(c.valid && c.blend==0 && c.r==499 && c.b==422 && c.key==160 && c.white==576);
    /* Across dark/clipped/colour-biased synthetic scenes: bounded output and
     * no division by zero; single-channel scenes must retain the base. */
    for (unsigned r=0;r<=4095;r+=257) for (unsigned g=0;g<=4095;g+=257)
    for (unsigned b=0;b<=4095;b+=257) {
        m=(isp_auto_meter){0};add(&m,r,g,b,1024,16);c=decide(&m);
        assert(c.r>=256 && c.r<=1024 && c.b>=256 && c.b<=1024);
        assert(c.g==256 && c.key>=160 && c.key<=176 && c.white>=384 && c.white<=576);
        if (!r || !g || !b) assert(!c.valid);
    }
    /* Probe grid on the active image (review Stop B R1): the four Bayer codes
     * with raw sizes that all give a 64x32 active image, each trimming only
     * its own axes; a large frame; the clamp to W/2 x H/2 on a small one. */
    {
        static const struct { uint32_t w, h, bayer, nx, ny; } g[] = {
            {64, 32, 0, 32, 16}, {65, 32, 1, 32, 16}, {64, 33, 2, 32, 16}, {65, 33, 3, 32, 16},
            {66, 34, 0, 32, 16}, {66, 34, 1, 32, 16}, {66, 34, 2, 32, 16}, {66, 34, 3, 32, 16},
            {2688, 1520, 3, 32, 16}, {40, 20, 0, 20, 10}, {40, 20, 1, 19, 10}, {40, 20, 2, 20, 9},
            {40, 20, 3, 19, 9}};
        for (unsigned i = 0; i < sizeof g / sizeof g[0]; ++i) {
            uint32_t nx, ny;
            isp_auto_probe_grid(g[i].w, g[i].h, g[i].bayer, &nx, &ny);
            if (nx != g[i].nx || ny != g[i].ny) {
                printf("probe grid %ux%u bayer %u: %ux%u, expected %ux%u\n", g[i].w, g[i].h, g[i].bayer, nx, ny,
                       g[i].nx, g[i].ny);
                return 1;
            }
        }
    }
    puts("ISP auto policy: PASS (fallback, neutral/LED, 4096 colour cases, probe grid of 13 geometries)");
    return 0;
}
