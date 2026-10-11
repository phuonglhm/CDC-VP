"""Decode generated I/P pictures with an independent external decoder."""
import pathlib, subprocess, sys
exe, ffmpeg, root, qp = sys.argv[1:]
root=pathlib.Path(root);root.mkdir(parents=True,exist_ok=True)
prefix=root/('codec_qp'+qp)
subprocess.run([exe,'generate',str(prefix),qp],check=True)
decoded=str(prefix)+'.decoded.yuv'
r=subprocess.run([ffmpeg,'-nostdin','-v','error','-xerror','-err_detect','explode','-y','-i',str(prefix)+'.h264','-pix_fmt','yuv420p','-f','rawvideo',decoded],capture_output=True,text=True)
if r.returncode or r.stderr.strip():
    raise RuntimeError('Decoder reported an error: '+r.stderr)
a=pathlib.Path(str(prefix)+'.yuv').read_bytes();b=pathlib.Path(decoded).read_bytes()
if a!=b:
    first=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
    raise AssertionError(f'Reconstruction mismatch at byte {first}: sizes {len(a)}/{len(b)}, expected {a[first:first+8].hex()}, got {b[first:first+8].hex()}')
assert len(a)==4*32*32*3//2
print(f'PASS: QP {qp}, 4 I/P pictures, decoder reconstruction matches all Y/U/V bytes')
