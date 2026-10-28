#!/usr/bin/python3 -I
# Plan T19: the GDB server of vp_fu3_soc over its remote protocol, both CPUs,
# with the T19 firmware pair (tests/fw/t19_ibex.c, t19_cva6.c).
#
#   gdb_rsp_test.py VP_FU3_SOC FW_DIR NM {session|kill|nowait}
#
# session: attach to both harts at reset, a hardware breakpoint hit in both
#   boots around the Ibex software reset (trigger re-armed), a software
#   breakpoint after it (dcsr.ebreakm re-armed), CVA6 attach completing at its
#   release, simulated time frozen while both are stopped, CVA6 FP registers
#   and fflags read, fa2 written (the firmware prints it), trigger count,
#   memory and its errors, stub-owned CSRs refused, single step, ^C, a second
#   client refused, detach and the run ending with PASS.
# kill: k ends the run (exit 0, reported). nowait: no client, the run goes on.
# checksum: damaged packets NACKed and not executed, recovery, NACK resend.
# gdb: riscv-none-elf-gdb itself (beside NM) on CVA6 in batch mode: target
#   description, breakpoint, FP registers as floats, register write, stepi, detach.
import re
import socket
import subprocess
import sys
import threading
import time

VP, FW, NM, CASE = sys.argv[1:5]
checks = failures = 0


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
        print("FAIL:", what, flush=True)


class Rsp:
    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=60)
        self.buf = b""

    def send(self, body):
        data = body.encode("latin-1")
        self.sock.sendall(b"$" + data + b"#%02x" % (sum(data) & 0xFF))

    def send_raw(self, data):
        self.sock.sendall(data)

    def recv(self):
        while True:
            self.buf = self.buf.lstrip(b"+")
            start = self.buf.find(b"$")
            end = self.buf.find(b"#", start + 1) if start >= 0 else -1
            if start >= 0 and end >= 0 and len(self.buf) >= end + 3:
                raw = self.buf[start + 1:end]
                self.buf = self.buf[end + 3:]
                out, i = bytearray(), 0
                while i < len(raw):
                    if raw[i] == 0x7D:
                        out.append(raw[i + 1] ^ 0x20)
                        i += 2
                    else:
                        out.append(raw[i])
                        i += 1
                return out.decode("latin-1")
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("connection closed")
            self.buf += chunk

    def cmd(self, body):
        self.send(body)
        return self.recv()

    def reg(self, n):
        r = self.cmd("p%x" % n)
        return int.from_bytes(bytes.fromhex(r), "little") if not r.startswith("E") else None

    def set_reg(self, n, v):
        return self.cmd("P%x=%s" % (n, v.to_bytes(4, "little").hex()))

    def mem(self, addr, n):
        r = self.cmd("m%x,%x" % (addr, n))
        return None if r.startswith("E") else bytes.fromhex(r)

    def kind(self, addr):
        return 4 if (self.mem(addr, 2)[0] & 3) == 3 else 2


def symbols(elf):
    out = subprocess.run([NM, elf], capture_output=True, text=True, check=True).stdout
    return {f[2]: int(f[0], 16) for f in (l.split() for l in out.splitlines()) if len(f) == 3}


def start(args):
    vp = subprocess.Popen([VP] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    lines = []

    def collect():
        for line in vp.stdout:
            lines.append(line)

    threading.Thread(target=collect, daemon=True).start()
    return vp, lines


def port_of(lines, name, timeout=30):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for l in list(lines):
            m = re.search(r"GDB server for %s \(hart \d\) on 127\.0\.0\.1:(\d+)" % name, l)
            if m:
                return int(m.group(1))
        time.sleep(0.05)
    raise RuntimeError("no GDB port for " + name)


def finish(vp, lines):
    try:
        rc = vp.wait(timeout=120)
    except subprocess.TimeoutExpired:
        vp.kill()
        rc = None
    time.sleep(0.2)
    return rc, "".join(lines)


PC, A0, FA0, FFLAGS, CSR = 32, 10, 33 + 10, 65 + 1, 65
IBEX_ELF, CVA6_ELF = FW + "/fu3_t19_ibex.elf", FW + "/fu3_t19_cva6.elf"


def session():
    si, sc = symbols(IBEX_ELF), symbols(CVA6_ELF)
    vp, lines = start(["--rom", IBEX_ELF, "--elf", CVA6_ELF, "--gdb-ibex", "0", "--gdb-cva6", "0",
                       "--expect-uart", "PASS"])
    ibex, cva6 = Rsp(port_of(lines, "Ibex")), Rsp(port_of(lines, "CVA6"))
    for c in (ibex, cva6):
        check(c.cmd("qSupported:swbreak+;hwbreak+").startswith("PacketSize="), "qSupported")
        check(c.cmd("QStartNoAckMode") == "OK", "no-ack mode")
    xi, xc = ibex.cmd("qXfer:features:read:target.xml:0,3000"), cva6.cmd("qXfer:features:read:target.xml:0,3000")
    check("riscv.cpu" in xi and "riscv.fpu" not in xi and "mstatus" in xi, "Ibex target description: no FPU")
    check("riscv.fpu" in xc and '"fcsr"' in xc and '"ft0"' in xc, "CVA6 target description: F registers and fcsr")

    # Attach: Ibex halts on its first instruction after its release.
    check(ibex.cmd("?") == "S05", "Ibex stop reply after attach")
    check(ibex.reg(PC) == 0x80, "Ibex stopped at the reset vector 0x80")
    cva6.send("?")   # answered when CVA6 is released and halted (Ibex runs first)

    # Hardware breakpoint in both boots: the trigger is re-armed after the software reset.
    mark = si["t19_boot_mark"]
    check(ibex.cmd("Z1,%x,2" % mark) == "OK", "Z1 on t19_boot_mark")
    check(ibex.cmd("c") == "T05hwbreak:;", "boot 1 stops on the trigger")
    check(ibex.reg(PC) == mark and ibex.reg(A0) == 1, "boot 1: pc t19_boot_mark, a0 1")
    # Like GDB: step off a breakpoint before continuing (an execute trigger
    # fires before its instruction, so resuming on it stops again at once).
    check(ibex.cmd("z1,%x,2" % mark) == "OK" and ibex.cmd("s") == "T05", "step off the breakpoint")
    check(ibex.cmd("Z1,%x,2" % mark) == "OK", "Z1 again")
    check(ibex.cmd("c") == "T05hwbreak:;", "boot 2 (after the software reset) stops on the re-armed trigger")
    check(ibex.reg(PC) == mark and ibex.reg(A0) == 2, "boot 2: pc t19_boot_mark, a0 2")
    check(ibex.cmd("z1,%x,2" % mark) == "OK", "z1")
    # Software breakpoint after the reset: dcsr.ebreakm re-armed.
    after = si["t19_after_handoff"]
    original, kind = ibex.mem(after, 4), ibex.kind(after)
    check(ibex.cmd("Z0,%x,%x" % (after, kind)) == "OK", "Z0 on t19_after_handoff")
    check(ibex.mem(after, kind) != original[:kind], "Z0 patched an EBREAK into the code")
    check(ibex.cmd("c") == "T05swbreak:;" and ibex.reg(PC) == after, "software breakpoint hit after the reset")
    check(ibex.cmd("z0,%x,%x" % (after, kind)) == "OK", "z0")
    check(ibex.mem(after, 4) == original, "z0 restored the original code")

    # CVA6 attach completes at its release (reset vector 0x10000).
    check(cva6.recv() == "S05", "CVA6 stop reply once released")
    check(cva6.reg(PC) == 0x10000, "CVA6 stopped at its reset vector 0x10000")

    # Both stopped: simulated time stands still.
    c1, i1 = cva6.reg(CSR + 0xB00), ibex.reg(CSR + 0xB00)
    time.sleep(0.5)
    check(cva6.reg(CSR + 0xB00) == c1 and ibex.reg(CSR + 0xB00) == i1 and c1 is not None,
          "mcycle of both harts unchanged while both are stopped")

    # CVA6 FP state at t19_fp_stop.
    stop = sc["t19_fp_stop"]
    stop_kind = cva6.kind(stop)
    check(cva6.cmd("Z0,%x,%x" % (stop, stop_kind)) == "OK", "Z0 on t19_fp_stop")
    check(cva6.cmd("c") == "T05swbreak:;" and cva6.reg(PC) == stop, "CVA6 stops at t19_fp_stop")
    check(cva6.reg(FA0) == 0x3FC00000 and cva6.reg(FA0 + 1) == 0x40100000, "fa0 1.5, fa1 2.25")
    check(cva6.reg(FA0 + 2) == 0x40580000, "fa2 = fa0 * fa1 = 3.375")
    check(cva6.reg(FA0 + 4) == 0x3EAAAAAB, "fa4 = 1/3")
    check((cva6.reg(FFLAGS) or 0) & 1 == 1 and (cva6.reg(CSR + 3) or 0) & 1 == 1, "fflags/fcsr NX after 1/3")
    check(cva6.set_reg(FA0 + 2, 0x41200000) == "OK" and cva6.reg(FA0 + 2) == 0x41200000, "fa2 written (10.0)")
    check(ibex.cmd("p%x" % FA0) == "E01", "Ibex has no FP register")
    check(cva6.cmd("z0,%x,%x" % (stop, stop_kind)) == "OK", "z0 on t19_fp_stop")

    # Two execute triggers on CVA6.
    check(cva6.cmd("Z1,10000,2") == "OK" and cva6.cmd("Z1,10004,2") == "OK", "two hardware breakpoints")
    check(cva6.cmd("Z1,10008,2") == "E01", "third hardware breakpoint refused (CVA6 has 2 triggers)")
    check(cva6.cmd("z1,10000,2") == "OK" and cva6.cmd("z1,10004,2") == "OK", "hardware breakpoints removed")

    # Memory through the hart's bus port.
    check(cva6.mem(0x0103F204, 4) == bytes([2, 0, 0, 0]), "DSRAM boot counter 2 read by CVA6")
    check(cva6.cmd("M103f300,4:efbeadde") == "OK" and ibex.mem(0x0103F300, 4) == bytes.fromhex("efbeadde"),
          "memory written by one, read by the other")
    check(ibex.mem(0x44010100, 4) == (0x10000).to_bytes(4, "little"), "MMIO read: VP_ABI_VERSION")
    check(cva6.mem(0x20000000, 4) is None, "unmapped read answers an error")
    check(cva6.cmd("Z0,30000000,4") == "E01", "no software breakpoint in the QSPI XIP window")
    status = cva6.mem(0x44010070, 4)
    check(cva6.cmd("Z0,44010070,4") == "E01" and cva6.mem(0x44010070, 4) == status,
          "no software breakpoint on a register that ignores the write (SYSCSR STATUS)")
    check(cva6.cmd("P%x=00000000" % (CSR + 0x7B0)) == "E01", "dcsr belongs to the stub")

    # Single step, then ^C on a running hart.
    check(cva6.cmd("s") == "T05" and cva6.reg(PC) not in (None, stop), "single step moves pc")
    cva6.send("c")
    time.sleep(0.3)
    cva6.send_raw(b"\x03")
    check(cva6.recv() == "T02", "^C stops a running CVA6 (T02)")

    # One client per hart.
    extra = socket.create_connection(("127.0.0.1", ibex.sock.getpeername()[1]), timeout=30)
    extra.settimeout(30)
    try:
        closed = extra.recv(16) == b""
    except OSError:
        closed = True
    check(closed, "a second client on the Ibex port is closed")

    # Detach removes what the stub set: a software breakpoint on code Ibex runs afterwards.
    put = si["t19_report"]
    check(ibex.cmd("Z0,%x,%x" % (put, ibex.kind(put))) == "OK", "Z0 on t19_report left for detach")
    check(ibex.cmd("D") == "OK" and cva6.cmd("D") == "OK", "detach both")
    rc, out = finish(vp, lines)
    check(rc == 0 and "vp_fu3_soc: PASS" in out, "run ends with PASS after detach (rc %s)" % rc)
    check("[UART0] T19 boot 1" in out and "[UART0] T19 boot 2" in out, "both boots printed")
    check("cva6 result 0x41200000" in out, "CVA6 used the fa2 GDB wrote")
    return out


def kill():
    vp, lines = start(["--rom", IBEX_ELF, "--gdb-ibex", "0"])
    ibex = Rsp(port_of(lines, "Ibex"))
    check(ibex.cmd("?") == "S05", "attached")
    ibex.send("k")
    rc, out = finish(vp, lines)
    check(rc == 0 and "run ended by a GDB kill" in out, "k ends the run (rc %s)" % rc)
    return out


def frame(body):
    data = body.encode("latin-1")
    return b"$" + data + b"#%02x" % (sum(data) & 0xFF)


def checksum():
    # Review R03, acknowledgement mode (default): a damaged packet is NACKed
    # and not executed; the next valid one works; a NACK from the client gets
    # the last reply again.
    vp, lines = start(["--rom", IBEX_ELF, "--gdb-ibex", "0"])
    s = socket.create_connection(("127.0.0.1", port_of(lines, "Ibex")), timeout=30)
    buf = b""

    def read_exact(n):
        nonlocal buf
        while len(buf) < n:
            chunk = s.recv(4096)
            if not chunk:
                raise ConnectionError("closed")
            buf += chunk
        out, buf = buf[:n], buf[n:]
        return out

    def read_packet():
        nonlocal buf
        while b"#" not in buf or len(buf) < buf.index(b"#") + 3:
            chunk = s.recv(4096)
            if not chunk:
                raise ConnectionError("closed")
            buf += chunk
        end = buf.index(b"#") + 3
        out, buf = buf[:end], buf[end:]
        return out

    def exchange(body):
        s.sendall(frame(body))
        check(read_exact(1) == b"+", "ACK for " + body)
        return read_packet()

    def x5():
        return int.from_bytes(bytes.fromhex(exchange("p5")[1:-3].decode()), "little")

    check(exchange("?") == frame("S05"), "attached")
    before = x5()
    good = frame("P5=78563412")
    bad = good[:-2] + b"%02x" % ((int(good[-2:], 16) + 1) & 0xFF)
    s.sendall(bad)
    check(read_exact(1) == b"-", "packet with a wrong checksum is NACKed")
    s.sendall(b"$P5=78563412#zz")
    check(read_exact(1) == b"-", "packet with a non-hex checksum is NACKed")
    s.settimeout(0.5)
    try:
        extra = s.recv(64)
    except socket.timeout:
        extra = b""
    s.settimeout(30)
    check(extra == b"", "no reply to a damaged packet")
    check(x5() == before, "x5 unchanged by the damaged packets")
    check(exchange("P5=78563412") == frame("OK") and x5() == 0x12345678, "the next valid packet is executed")
    reply = exchange("p5")
    s.sendall(b"-")
    check(read_packet() == reply, "a NACK from the client gets the last reply again")
    s.sendall(frame("k"))
    rc, out = finish(vp, lines)
    check(rc == 0 and "bad checksum dropped" in out, "run ended by kill, damaged packets reported (rc %s)" % rc)
    return out


def nowait():
    vp, lines = start(["--rom", IBEX_ELF, "--elf", CVA6_ELF, "--gdb-ibex", "0", "--gdb-no-wait", "--expect-uart",
                       "PASS", "--timeout-ms", "50"])
    rc, out = finish(vp, lines)
    check(rc == 0 and "cva6 result 0x40580000" in out, "--gdb-no-wait: run without a client (rc %s)" % rc)
    return out


def real_gdb():
    gdb = NM[: -len("nm")] + "gdb"
    vp, lines = start(["--rom", IBEX_ELF, "--elf", CVA6_ELF, "--gdb-cva6", "0", "--expect-uart", "PASS"])
    port = port_of(lines, "CVA6")
    script = ["set confirm off", "set remotetimeout 30", "target remote 127.0.0.1:%d" % port,
              "info registers pc", "break t19_fp_stop", "continue", "info registers pc",
              "info registers fa0 fa1 fa2 fa4", "p/x $fflags", "set $fa2 = 10", "p $fa2", "stepi",
              "info registers pc", "p/x $misa", "detach"]
    args = [gdb, "-nx", "-batch"] + [a for c in script for a in ("-ex", c)] + [CVA6_ELF]
    g = subprocess.run(args, capture_output=True, text=True, timeout=120)
    text = g.stdout + g.stderr
    check(re.search(r"pc\s+0x10000\b", text) is not None, "gdb: CVA6 stopped at 0x10000 after attach")
    stop = symbols(CVA6_ELF)["t19_fp_stop"]
    check(("Breakpoint 1, 0x%08x" % stop) in text, "gdb: breakpoint at t19_fp_stop hit")
    check(re.search(r"fa0\s+1\.5\b", text) and re.search(r"fa1\s+2\.25\b", text) and
          re.search(r"fa2\s+3\.375\b", text) and re.search(r"fa4\s+0\.333333", text), "gdb: FP registers as floats")
    check(re.search(r"= 0x1\b", text) is not None, "gdb: fflags NX")
    check(re.search(r"\$\d+ = 10\b", text) is not None, "gdb: fa2 written")
    check(re.search(r"\$3 = 0x40001124\b", text) is not None, "gdb: misa RV32IMFC (0x40001124)")
    check("Detaching from program" in text or "[Inferior 1 (Remote target) detached]" in text, "gdb: detach")
    rc, out = finish(vp, lines)
    check(rc == 0 and "cva6 result 0x41200000" in out, "run ends with PASS, CVA6 used fa2 = 10.0 (rc %s)" % rc)
    return text + out


out = {"session": session, "kill": kill, "nowait": nowait, "gdb": real_gdb, "checksum": checksum}[CASE]()
if failures:
    print(out)
print("gdb rsp %s: %d checks, %d failures -> %s" % (CASE, checks, failures, "FAIL" if failures else "PASS"))
sys.exit(1 if failures else 0)
