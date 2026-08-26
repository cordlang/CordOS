#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

IMG=/tmp/cordos-disk.img
cp -f build/disk.img "$IMG"
QMP=/tmp/cordos-qmp.sock
rm -f "$QMP"

qemu-system-x86_64 \
  -drive file="$IMG",format=raw,if=ide,media=disk \
  -boot order=c \
  -display none \
  -qmp unix:"$QMP",server,nowait \
  -S &
QPID=$!
sleep 1

python3 - <<'PY'
import socket, json, time, select

s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/cordos-qmp.sock")
s.settimeout(2.0)
buf = b""

def recv_obj():
    global buf
    while True:
        if b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.strip()
            if not line:
                continue
            return json.loads(line.decode())
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            return None
        if not chunk:
            return None
        buf += chunk

def send(cmd):
    s.sendall((json.dumps(cmd) + "\n").encode())
    while True:
        o = recv_obj()
        if o is None:
            return None
        if "event" in o:
            continue
        return o

recv_obj()  # greeting
send({"execute": "qmp_capabilities"})
send({"execute": "cont"})
time.sleep(2.5)

# Drain pending events
s.settimeout(0.3)
while True:
    o = recv_obj()
    if o is None:
        break

s.settimeout(3.0)
vga = send({"execute": "human-monitor-command",
            "arguments": {"command-line": "xp /100c 0xb8000"}})
regs = send({"execute": "human-monitor-command",
             "arguments": {"command-line": "info registers"}})
mbr = send({"execute": "human-monitor-command",
            "arguments": {"command-line": "x /16bx 0x7c00"}})

def chars_from_xp(text):
    # pull printable chars from xp /Nc output
    out = []
    for part in text.replace("\\x07", "").split("'"):
        if len(part) == 1 and part.isprintable():
            out.append(part)
        elif part.startswith("\\x"):
            pass
    return "".join(out)

raw = vga.get("return", "") if vga else ""
print("VGA:", chars_from_xp(raw)[:120])
print("MBR:", (mbr or {}).get("return", "")[:120])
r = (regs or {}).get("return", "")
for line in r.splitlines()[:8]:
    print(line)
send({"execute": "quit"})
PY

wait "$QPID" 2>/dev/null || true
