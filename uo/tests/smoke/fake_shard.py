#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""A minimal login + game server speaking just enough of the UO protocol for a smoke test:
account login, shard list, relay, character list, enter world at (1440, 1690), a nearby
mobile, an item, a welcome message, and walk confirmations. Single client, single thread."""
import os
import socket
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = [int(v) for v in open(os.path.join(HERE, "huffman_tree.txt")).read().splitlines()[1].split(",")]


def build_codes():
    codes = {}
    stack = [(0, 0, 0)]
    while stack:
        node, bits, length = stack.pop()
        for bit in (1, 0):
            child = TREE[node * 2 + (0 if bit else 1)]
            nb, nl = (bits << 1) | bit, length + 1
            if child <= 0:
                codes[-child] = (nb, nl)
            else:
                stack.append((child, nb, nl))
    return codes


CODES = build_codes()


def compress(packet):
    acc, n, out = 0, 0, bytearray()
    for sym in list(packet) + [256]:
        bits, length = CODES[sym]
        for i in range(length - 1, -1, -1):
            acc = (acc << 1) | ((bits >> i) & 1)
            n += 1
            if n == 8:
                out.append(acc)
                acc, n = 0, 0
    if n:
        out.append(acc << (8 - n))
    return bytes(out)


def var(pid, body):
    return bytes([pid]) + struct.pack(">H", len(body) + 3) + body


def recv_exact(conn, n):
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            raise ConnectionError("client closed")
        data += chunk
    return data


PLAYER = 0x00000100
X, Y = 1440, 1690

# Fixed-size client packets the shard reads past: single/double click, status request, war
# mode, view range.
IGNORED_FIXED = {0x09: 5, 0x06: 5, 0x34: 10, 0x72: 5, 0xC8: 2}


def login_server(conn, port):
    first = recv_exact(conn, 1)
    if first[0] == 0xEF:
        recv_exact(conn, 20)
    else:
        recv_exact(conn, 3)
    pkt = recv_exact(conn, 62)
    assert pkt[0] == 0x80, hex(pkt[0])
    print("login:", pkt[1:31].split(b"\0")[0].decode())
    body = struct.pack(">BH", 0x5D, 1) + struct.pack(">H", 0) + b"Smoke Shard".ljust(32, b"\0") + bytes([0, 0]) + bytes([1, 0, 0, 127])
    conn.sendall(var(0xA8, body))
    pkt = recv_exact(conn, 3)
    assert pkt[0] == 0xA0
    conn.sendall(bytes([0x8C, 127, 0, 0, 1]) + struct.pack(">HI", port, 0xC0FFEE))


def game_server(conn):
    key = recv_exact(conn, 4)
    pkt = recv_exact(conn, 65)
    assert pkt[0] == 0x91, hex(pkt[0])
    chars = bytes([1]) + b"Lord Smoke".ljust(30, b"\0") + b"".ljust(30, b"\0")
    chars += b"\0" * 60 * 4 + bytes([0]) + struct.pack(">I", 0)
    conn.sendall(compress(var(0xA9, chars)))
    pkt = recv_exact(conn, 73)
    assert pkt[0] == 0x5D
    print("playing:", pkt[5:35].split(b"\0")[0].decode())

    out = [
        bytes([0x1B]) + struct.pack(">IIHHHhB", PLAYER, 0, 0x0190, X, Y, 0, 4) + b"\0" * 19,
        var(0x78, struct.pack(">IHHHbBHBB", 0x200, 0x0190, X + 2, Y - 1, 0, 2, 0, 0, 6) + struct.pack(">I", 0)),
        var(0x1A, struct.pack(">IHHH", 0x40000001, 0x0011, X - 2, Y + 2) + bytes([0])),
        # A house (multi 1): its crate ring comes from multi.mul, not from the packet.
        var(0x1A, struct.pack(">IHHH", 0x40000002, 0x4001, X + 4, Y - 4) + bytes([0])),
        var(0xAE, struct.pack(">IHBHH", 0xFFFFFFFF, 0xFFFF, 0, 0x3B2, 3) + b"ENU\0" + b"System".ljust(30, b"\0")
            + "Welcome to AxmolUO.".encode("utf-16-be") + b"\0\0"),
        bytes([0x55]),
    ]
    for p in out:
        conn.sendall(compress(p))

    buf = b""
    while True:
        chunk = conn.recv(4096)
        if not chunk:
            print("client left")
            return
        buf += chunk
        while buf:
            pid = buf[0]
            if pid == 0x02 and len(buf) >= 7:
                seq = buf[2]
                print("walk dir=%d seq=%d" % (buf[1], seq))
                conn.sendall(compress(bytes([0x22, seq, 1])))
                buf = buf[7:]
            elif pid in (0x73,) and len(buf) >= 2:
                conn.sendall(compress(buf[:2]))
                buf = buf[2:]
            elif pid in IGNORED_FIXED and len(buf) >= IGNORED_FIXED[pid]:
                # Clicks and status requests the client sends on entering the world; without
                # skipping them the loop stalls and walk requests after them go unconfirmed.
                buf = buf[IGNORED_FIXED[pid]:]
            elif pid in (0xBD, 0xAD, 0x03, 0xBF, 0xD7) and len(buf) >= 3:
                n = struct.unpack(">H", buf[1:3])[0]
                if len(buf) < n:
                    break
                buf = buf[n:]
            else:
                break


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 2593
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(2)
    print("fake shard on", port, flush=True)
    conn, _ = srv.accept()
    login_server(conn, port)
    conn.close()
    conn, _ = srv.accept()
    try:
        game_server(conn)
    except ConnectionError as e:
        print(e)


if __name__ == "__main__":
    main()
