#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prepare a transient auto job, or capture the exact FW-selected CSR list.
The shared input job/profile are never edited. This does not estimate WB on
host; statistics and decisions come from firmware on the virtual CPU.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import sys
sys.dont_write_bytecode = True
from isp_pack import MODE_LEGACY, PackError, read_job

NAMES = ('WB_GAIN_R', 'WB_GAIN_G', 'WB_GAIN_B', 'GTM_KEY', 'GTM_LWHITE', 'EE_BETA')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    sub=ap.add_subparsers(dest='command', required=True)
    p=sub.add_parser('prepare');p.add_argument('job');p.add_argument('out')
    p=sub.add_parser('capture');p.add_argument('dir');p.add_argument('profile')
    a=ap.parse_args()
    if a.command=='prepare':
        raw=Path(a.job).read_bytes();job=read_job(a.job);n=len(raw)//4
        data=list(struct.unpack('<%dI'%n,raw))
        render=max(2,job['frames'])
        if render>=0xffffffff:raise PackError('auto job frame count overflows')
        data[11]=render+1
        # `isp run auto` runs in legacy mode (its policy rewrites WB/GTM
        # between frames): a version-2 job says so, so that the packer
        # expects that mode (job version 1 is legacy already).
        if n>12:data[12]=MODE_LEGACY
        Path(a.out).write_bytes(struct.pack('<%dI'%n,*data))
        print('ISP auto: 1 AWB probe + %d render frames (%d physical frames)'%(render,render+1))
        return
    d=Path(a.dir);text=(d/'platform.log').read_text(errors='replace')
    policy=re.findall(r'^ISP_AUTO (\S+) valid=([01]) zones=(\d+) blend=(\d+)\r?$',text,re.M)
    entries=re.findall(r'^ISP_AUTO_CSR ([A-Z_]+) (0x[0-9a-fA-F]+)\r?$',text,re.M)
    if len(policy)!=1 or len(entries)!=6 or set(n for n,_ in entries)!=set(NAMES):
        raise PackError('missing or ambiguous firmware auto decision')
    values={n:int(v,0) for n,v in entries}
    grid=re.findall(r'^ISP_AUTO_PROBE_GRID (\d+)x(\d+)\r?$',text,re.M)  # ISP-UPD-003: older firmware prints none
    rgrid=re.findall(r'^ISP_AUTO_RENDER_GRID (\d+)x(\d+)\r?$',text,re.M)
    if any(not 1<=values[n]<=0xfff for n in NAMES[:3]) or values['GTM_KEY']>0xff or values['GTM_LWHITE']>0xffff or values['EE_BETA']>0x8000:
        raise PackError('firmware auto CSR outside supported range')
    # ISP-UPD-008: the profile may use register or field syntax; each line is one
    # write (csrw_codec). The writes of the six controls become the firmware's
    # values as whole-register lines; every other line and the order are kept.
    here=Path(__file__).resolve().parent
    for p in (here, here.parents[3]/'components'/'FX1_Components'/'isp'/'tools' if len(here.parents)>3 else here):
        sys.path.insert(0,str(p))
    import csrw_codec
    cmap=csrw_codec.CsrMap()
    lines=csrw_codec.lines(a.profile);found=set()  # the lines the readers see (BOM, '\n' only)
    for i,l in enumerate(lines):
        body,sep,comment=l.partition('#')
        try:
            r=csrw_codec.parse_line(cmap,body,'%s:%d'%(a.profile,i+1))
        except csrw_codec.CsrwError as e:
            raise PackError(str(e))
        if r and r[0].name in values:
            found.add(r[0].name);lines[i]='%s 0x%X'%(r[0].name,values[r[0].name])+('  #'+comment if sep else '')
    if found!=set(NAMES):raise PackError('base profile missing auto controls')
    (d/'resolved_profile.csrw').write_text('# Firmware-selected controls; reference/debug artifact.\n'+'\n'.join(lines)+'\n',encoding='utf-8',errors='surrogateescape')
    total=read_job(d/'job.bin')['frames']
    (d/'auto.json').write_text(json.dumps(dict(policy=policy[0][0],valid=bool(int(policy[0][1])),neutral_zones=int(policy[0][2]),blend=int(policy[0][3]),csr=values,probe_awb_grid=('%sx%s'%grid[0] if len(grid)==1 else None),render_awb_grid=('%sx%s'%rgrid[0] if len(rgrid)==1 else None),probe_frames=1,render_frames=total-1,total_frames=total),indent=2)+'\n')
    print('Auto controls saved: '+str(d/'auto.json'))

if __name__=='__main__':
    try:main()
    except (PackError,OSError,ValueError) as e:raise SystemExit('isp_auto: '+str(e))
