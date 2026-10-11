"""Decode actual DDR output from the complete VP assembly, including recovery."""
import pathlib, subprocess, sys, re
exe,decoder,root,width,qp,scenario=sys.argv[1:]
root=pathlib.Path(root);root.mkdir(parents=True,exist_ok=True)
prefix=root/f'full_{width}_{qp}_{scenario}'
subprocess.run([exe,width,qp,str(prefix),scenario],check=True)
class Bits:
    def __init__(self,data): self.data=data; self.pos=0
    def read(self,n):
        value=0
        for _ in range(n):
            assert self.pos<len(self.data)*8, 'Truncated syntax'
            value=(value<<1)|((self.data[self.pos//8]>>(7-self.pos%8))&1);self.pos+=1
        return value
    def ue(self):
        n=0
        while self.read(1)==0:
            n+=1;assert n<32
        return (1<<n)-1+self.read(n)
    def se(self):
        v=self.ue();return (v+1)//2 if v&1 else -(v//2)
def check_headers(data):
    units=[n for n in re.split(b'\x00\x00(?:\x00)?\x01',data) if n]
    slices=0
    for unit in units:
        kind=unit[0]&31
        b=Bits(unit[1:].replace(b'\x00\x00\x03',b'\x00\x00'))
        special=scenario in ('syntax','syntax_negative')
        fn,poc=(6,7) if special else (4,4)
        if kind==7:
            assert b.read(8)==66; b.read(16);assert b.ue()==0
            assert b.ue()+4==fn and b.ue()==0 and b.ue()+4==poc
            assert b.ue()==1 and b.read(1)==0
            assert b.ue()==1 and b.ue()==1
            assert b.read(1)==1; b.read(1)
            assert b.read(1)==int(special)
            if special: assert [b.ue() for _ in range(4)]==[0,0,0,2]
        elif kind in (1,5):
            assert b.ue()==0
            slice_type=b.ue();assert slice_type==(2 if slices==0 else 0)
            assert b.ue()==0 and b.read(fn)==slices
            if kind==5: assert b.ue()==0
            assert b.read(poc)==2*slices
            if slice_type==0: assert b.read(2)==0
            assert b.read(2 if kind==5 else 1)==0
            delta=3 if scenario=='syntax' else -3 if scenario=='syntax_negative' else 0
            assert b.se()==delta
            filtered=scenario.startswith('filter')
            assert b.ue()==(0 if filtered else 1)
            if filtered:
                offsets=(2,1) if scenario=='filter_offsets' else (-2,-1) if scenario=='filter_negative' else (0,0)
                assert (b.se(),b.se())==offsets
            slices+=1
    assert slices==4
for run in range(2):
    stem=f'{prefix}_{run}'
    check_headers(pathlib.Path(stem+'.h264').read_bytes())
    result=subprocess.run([decoder,'-nostdin','-v','error','-xerror','-err_detect','explode','-y','-i',stem+'.h264','-pix_fmt','yuv420p','-f','rawvideo',stem+'.decoded.yuv'],capture_output=True,text=True)
    if result.returncode or result.stderr.strip(): raise RuntimeError(result.stderr)
    expected=pathlib.Path(stem+'.yuv').read_bytes()
    if scenario in ('syntax','syntax_negative'):
        cropped=bytearray()
        for f in range(4):
            base=f*1536
            cropped.extend(expected[base:base+32*28])
            cropped.extend(expected[base+1024:base+1024+16*14])
            cropped.extend(expected[base+1280:base+1280+16*14])
        expected=bytes(cropped)
    actual=pathlib.Path(stem+'.decoded.yuv').read_bytes()
    if expected!=actual:
        i=next((i for i,(a,b) in enumerate(zip(expected,actual)) if a!=b),min(len(expected),len(actual)))
        raise AssertionError(f'Decoder mismatch byte {i}, sizes {len(expected)}/{len(actual)}: {expected[i:i+8].hex()} vs {actual[i:i+8].hex()}')
    assert len(actual)==(5376 if scenario in ("syntax","syntax_negative") else 6144)
print(f'PASS DDR -> decoder: bus={width}, QP={qp}, scenario={scenario}, two 4-frame activations')
