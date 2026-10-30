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
    if any(not 1<=values[n]<=0xfff for n in NAMES[:3]) or values['GTM_KEY']>0xff or values['GTM_LWHITE']>0xffff or values['EE_BETA']>0x8000:
        raise PackError('firmware auto CSR outside supported range')
    lines=Path(a.profile).read_text().splitlines();found=set()
    for i,l in enumerate(lines):
        words=l.split()
        if words and words[0] in values:
            found.add(words[0]);lines[i]='%s 0x%X'%(words[0],values[words[0]])
    if found!=set(NAMES):raise PackError('base profile missing auto controls')
    (d/'resolved_profile.csrw').write_text('# Firmware-selected controls; reference/debug artifact.\n'+'\n'.join(lines)+'\n')
    total=read_job(d/'job.bin')['frames']
    (d/'auto.json').write_text(json.dumps(dict(policy=policy[0][0],valid=bool(int(policy[0][1])),neutral_zones=int(policy[0][2]),blend=int(policy[0][3]),csr=values,probe_frames=1,render_frames=total-1,total_frames=total),indent=2)+'\n')
    print('Auto controls saved: '+str(d/'auto.json'))

if __name__=='__main__':
    try:main()
    except (PackError,OSError,ValueError) as e:raise SystemExit('isp_auto: '+str(e))
