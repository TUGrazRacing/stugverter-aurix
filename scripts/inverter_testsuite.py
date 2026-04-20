#!/usr/bin/env python3
"""
Inverter Ethernet protocol testsuite.

This script exercises the full command set defined in protocol.md over TCP/UDP,
logs raw bytes sent/received, and reports pass/fail diagnostics per test step.
"""

from __future__ import annotations

import argparse
import dataclasses
import logging
import math
import socket
import struct
import sys
import time
from typing import Dict, List, Optional, Tuple

START_BYTES = b"\xAA\x55"
MAX_PAYLOAD = 250
TCP_HEADER_SIZE = 5
DICT_RECORD_LEN = 24
UDP_STREAM_PORT = 3040

CMD_DICTIONARY = 0x00
CMD_READ = 0x01
CMD_WRITE = 0x02
CMD_STREAM_START = 0x03
CMD_STREAM_STOP = 0x04
CMD_CONFIG_COMMIT = 0x06

CMD_DICTIONARY_ACK = 0x80
CMD_READ_ACK = 0x81
CMD_WRITE_ACK = 0x82
CMD_STREAM_ACK = 0x83
CMD_STREAM_STOP_ACK = 0x84
CMD_CONFIG_COMMIT_ACK = 0x86
CMD_ERROR = 0x8F

ERR_INVALID_ADDR = 0x01
ERR_ACCESS_DENIED = 0x02
ERR_OTHER = 0x03

ACCESS_READ_ONLY = 0x00
ACCESS_WRITE_AFTER_RESTART = 0x01
ACCESS_WRITE_LIVE = 0x02

TYPE_UINT8 = 0
TYPE_INT8 = 1
TYPE_UINT16 = 2
TYPE_INT16 = 3
TYPE_UINT32 = 4
TYPE_INT32 = 5
TYPE_FLOAT32 = 6
TYPE_UINT64 = 7

CMD_NAMES = {
    CMD_DICTIONARY: "Dict-Req",
    CMD_READ: "Read-Req",
    CMD_WRITE: "Write-Req",
    CMD_STREAM_START: "Stream-Req",
    CMD_STREAM_STOP: "Stream-Stop",
    CMD_CONFIG_COMMIT: "Update-Config",
    CMD_DICTIONARY_ACK: "Dict-Data",
    CMD_READ_ACK: "Read-Data",
    CMD_WRITE_ACK: "Write-Ack",
    CMD_STREAM_ACK: "Stream-Ack",
    CMD_STREAM_STOP_ACK: "Stream-Stop-Ack",
    CMD_CONFIG_COMMIT_ACK: "Update-Config-Ack",
    CMD_ERROR: "Error-Resp",
}

ERR_NAMES = {
    ERR_INVALID_ADDR: "Invalid Address",
    ERR_ACCESS_DENIED: "Access Denied",
    ERR_OTHER: "Other / malformed request",
}

TYPE_INFO = {
    TYPE_UINT8: (1, "<B", "uint8"),
    TYPE_INT8: (1, "<b", "int8"),
    TYPE_UINT16: (2, "<H", "uint16"),
    TYPE_INT16: (2, "<h", "int16"),
    TYPE_UINT32: (4, "<I", "uint32"),
    TYPE_INT32: (4, "<i", "int32"),
    TYPE_FLOAT32: (4, "<f", "float32"),
    TYPE_UINT64: (8, "<Q", "uint64"),
}


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def now_s() -> float:
    return time.monotonic()


@dataclasses.dataclass
class Frame:
    cmd: int
    payload: bytes
    raw: bytes


@dataclasses.dataclass
class DictEntry:
    address: int
    type_code: int
    access: int
    name: str
    unit: str

    @property
    def size(self) -> int:
        if self.type_code not in TYPE_INFO:
            return 0
        return TYPE_INFO[self.type_code][0]

    @property
    def type_name(self) -> str:
        if self.type_code not in TYPE_INFO:
            return f"unknown({self.type_code})"
        return TYPE_INFO[self.type_code][2]


@dataclasses.dataclass
class TestResult:
    name: str
    passed: bool
    details: str
    duration_s: float


class ProtocolError(Exception):
    pass


class ProtocolClient:
    def __init__(self, host: str, tcp_port: int, timeout_s: float, logger: logging.Logger):
        self.host = host
        self.tcp_port = tcp_port
        self.timeout_s = timeout_s
        self.logger = logger
        self.sock: Optional[socket.socket] = None
        self.rx_buf = bytearray()
        self.pending_frames: List[Frame] = []

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout_s)
        self.sock.connect((self.host, self.tcp_port))
        self.logger.info("TCP connected to %s:%d", self.host, self.tcp_port)

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None
        self.logger.info("TCP closed")

    def send_frame(self, cmd: int, payload: bytes = b"") -> None:
        if self.sock is None:
            raise ProtocolError("TCP socket not connected")
        if len(payload) > MAX_PAYLOAD:
            raise ProtocolError(f"payload too large: {len(payload)}")

        header = START_BYTES + bytes([cmd]) + struct.pack("<H", len(payload))
        frame = header + payload

        self.logger.debug("TX CMD=0x%02X (%s) LEN=%d", cmd, CMD_NAMES.get(cmd, "Unknown"), len(payload))
        self.logger.debug("TX RAW: %s", hex_bytes(frame))
        self.sock.sendall(frame)

    def _read_from_socket(self, timeout_s: float) -> bool:
        if self.sock is None:
            raise ProtocolError("TCP socket not connected")

        self.sock.settimeout(timeout_s)
        try:
            data = self.sock.recv(2048)
        except socket.timeout:
            return False

        if not data:
            raise ProtocolError("TCP connection closed by peer")

        self.logger.debug("RX CHUNK (%d): %s", len(data), hex_bytes(data))
        self.rx_buf.extend(data)
        return True

    def _extract_frames(self) -> List[Frame]:
        out: List[Frame] = []

        while len(self.rx_buf) >= TCP_HEADER_SIZE:
            if self.rx_buf[0] != START_BYTES[0] or self.rx_buf[1] != START_BYTES[1]:
                dropped = self.rx_buf[0]
                del self.rx_buf[0]
                self.logger.warning("RX resync dropped byte: 0x%02X", dropped)
                continue

            cmd = self.rx_buf[2]
            payload_len = self.rx_buf[3] | (self.rx_buf[4] << 8)
            if payload_len > MAX_PAYLOAD:
                self.logger.warning("RX invalid payload len=%d, dropping one byte for resync", payload_len)
                del self.rx_buf[0]
                continue

            frame_len = TCP_HEADER_SIZE + payload_len
            if len(self.rx_buf) < frame_len:
                break

            raw = bytes(self.rx_buf[:frame_len])
            payload = bytes(self.rx_buf[TCP_HEADER_SIZE:frame_len])
            del self.rx_buf[:frame_len]

            self.logger.debug(
                "RX FRAME CMD=0x%02X (%s) LEN=%d RAW=%s",
                cmd,
                CMD_NAMES.get(cmd, "Unknown"),
                payload_len,
                hex_bytes(raw),
            )
            out.append(Frame(cmd=cmd, payload=payload, raw=raw))

        return out

    def recv_frame(self, timeout_s: float) -> Frame:
        deadline = now_s() + timeout_s
        while now_s() < deadline:
            if self.pending_frames:
                return self.pending_frames.pop(0)

            frames = self._extract_frames()
            if frames:
                self.pending_frames.extend(frames)
                return self.pending_frames.pop(0)

            remaining = max(0.01, deadline - now_s())
            self._read_from_socket(min(0.25, remaining))

        raise TimeoutError(f"timeout waiting for frame ({timeout_s:.2f}s)")

    def recv_frames_until_silence(self, max_total_s: float, silence_s: float) -> List[Frame]:
        collected: List[Frame] = []
        end_time = now_s() + max_total_s
        last_rx = now_s()

        if self.pending_frames:
            collected.extend(self.pending_frames)
            self.pending_frames.clear()
            last_rx = now_s()

        while now_s() < end_time:
            frames = self._extract_frames()
            if frames:
                collected.extend(frames)
                last_rx = now_s()
                continue

            if collected and (now_s() - last_rx) >= silence_s:
                break

            remaining = max(0.0, end_time - now_s())
            if remaining <= 0.0:
                break

            got = self._read_from_socket(min(0.15, remaining))
            if got:
                last_rx = now_s()

        return collected


class InverterTestSuite:
    def __init__(self, args: argparse.Namespace, logger: logging.Logger):
        self.args = args
        self.logger = logger
        self.client = ProtocolClient(args.host, args.tcp_port, args.tcp_timeout, logger)
        self.results: List[TestResult] = []
        self.dictionary: List[DictEntry] = []
        self.dict_by_addr: Dict[int, DictEntry] = {}

    def run(self) -> int:
        self.logger.info("==== Inverter Ethernet Testsuite started ====")
        self.logger.info("Target TCP: %s:%d", self.args.host, self.args.tcp_port)
        self.logger.info("Target UDP listen: 0.0.0.0:%d", self.args.udp_port)

        try:
            self.client.connect()

            self._run_test("Dictionary request and parse", self.test_dictionary)
            self._run_test("Read request roundtrip", self.test_read)
            self._run_test("Write live parameter and restore", self.test_write_live)
            self._run_test("Write denied on read-only", self.test_write_denied)
            self._run_test("Invalid address errors", self.test_invalid_address_errors)
            self._run_test("Config write and commit", self.test_config_write_and_commit)
            self._run_test("Stream start, UDP receive, stream stop", self.test_stream_flow)
            self._run_test("Stream stop inactive id returns error", self.test_stream_stop_inactive)

        finally:
            self.client.close()

        return self._print_summary()

    def _run_test(self, name: str, fn) -> None:
        self.logger.info("---- TEST START: %s ----", name)
        start = now_s()
        try:
            detail = fn()
            duration = now_s() - start
            self.results.append(TestResult(name=name, passed=True, details=detail, duration_s=duration))
            self.logger.info("PASS: %s (%.3fs)", name, duration)
            self.logger.info("DETAIL: %s", detail)
        except Exception as exc:  # pylint: disable=broad-except
            duration = now_s() - start
            msg = f"{type(exc).__name__}: {exc}"
            self.results.append(TestResult(name=name, passed=False, details=msg, duration_s=duration))
            self.logger.error("FAIL: %s (%.3fs)", name, duration)
            self.logger.error("DETAIL: %s", msg)

    def _expect_frame(self, expected_cmds: Tuple[int, ...], timeout_s: float) -> Frame:
        while True:
            frame = self.client.recv_frame(timeout_s)
            if frame.cmd == CMD_ERROR:
                self._raise_error_frame(frame)
            if frame.cmd in expected_cmds:
                return frame
            self.logger.warning(
                "Ignoring unexpected frame CMD=0x%02X (%s)",
                frame.cmd,
                CMD_NAMES.get(frame.cmd, "Unknown"),
            )

    def _expect_error_frame(self, timeout_s: float) -> Tuple[int, int]:
        frame = self.client.recv_frame(timeout_s)
        if frame.cmd != CMD_ERROR:
            raise ProtocolError(
                f"expected Error-Resp, got CMD=0x{frame.cmd:02X} ({CMD_NAMES.get(frame.cmd, 'Unknown')})"
            )
        if len(frame.payload) != 3:
            raise ProtocolError(f"Error-Resp payload length must be 3, got {len(frame.payload)}")

        address = frame.payload[0] | (frame.payload[1] << 8)
        code = frame.payload[2]
        self.logger.info("Received Error-Resp addr=0x%04X code=0x%02X (%s)", address, code, ERR_NAMES.get(code, "Unknown"))
        return address, code

    @staticmethod
    def _raise_error_frame(frame: Frame) -> None:
        if len(frame.payload) == 3:
            address = frame.payload[0] | (frame.payload[1] << 8)
            code = frame.payload[2]
            raise ProtocolError(f"device Error-Resp addr=0x{address:04X}, code=0x{code:02X} ({ERR_NAMES.get(code, 'Unknown')})")
        raise ProtocolError(f"device Error-Resp malformed length={len(frame.payload)}")

    def test_dictionary(self) -> str:
        self.client.send_frame(CMD_DICTIONARY, b"")
        frames = self.client.recv_frames_until_silence(self.args.dict_total_timeout, self.args.dict_silence_timeout)

        dict_frames = [f for f in frames if f.cmd == CMD_DICTIONARY_ACK]
        err_frames = [f for f in frames if f.cmd == CMD_ERROR]

        if err_frames:
            self._raise_error_frame(err_frames[0])

        if not dict_frames:
            raise ProtocolError("no Dict-Data frames received")

        entries: List[DictEntry] = []
        for idx, frame in enumerate(dict_frames):
            if len(frame.payload) % DICT_RECORD_LEN != 0:
                raise ProtocolError(
                    f"dictionary frame #{idx} has invalid payload length {len(frame.payload)} (not multiple of {DICT_RECORD_LEN})"
                )

            for offset in range(0, len(frame.payload), DICT_RECORD_LEN):
                rec = frame.payload[offset: offset + DICT_RECORD_LEN]
                address = rec[0] | (rec[1] << 8)
                ctrl = rec[2]
                type_code = ctrl & 0x0F
                access = (ctrl >> 4) & 0x03
                name = rec[3:19].decode("ascii", errors="replace").rstrip(" ")
                unit = rec[19:24].decode("ascii", errors="replace").rstrip(" ")
                entries.append(DictEntry(address=address, type_code=type_code, access=access, name=name, unit=unit))

        if not entries:
            raise ProtocolError("dictionary parsed zero entries")

        self.dictionary = sorted(entries, key=lambda e: e.address)
        self.dict_by_addr = {e.address: e for e in self.dictionary}

        unknown_types = [e for e in self.dictionary if e.type_code not in TYPE_INFO]
        if unknown_types:
            addresses = ", ".join(f"0x{e.address:04X}" for e in unknown_types[:8])
            raise ProtocolError(f"dictionary contains unsupported type codes at addresses: {addresses}")

        ro_count = sum(1 for e in self.dictionary if e.access == ACCESS_READ_ONLY)
        live_count = sum(1 for e in self.dictionary if e.access == ACCESS_WRITE_LIVE)
        cfg_count = sum(1 for e in self.dictionary if e.access == ACCESS_WRITE_AFTER_RESTART)

        self.logger.info(
            "Dictionary parsed: total=%d, read-only=%d, write-live=%d, write-after-restart=%d",
            len(self.dictionary),
            ro_count,
            live_count,
            cfg_count,
        )
        return f"received {len(dict_frames)} Dict-Data frames and parsed {len(self.dictionary)} entries"

    def _build_read_payload(self, addresses: List[int]) -> bytes:
        return b"".join(struct.pack("<H", a) for a in addresses)

    def _decode_by_entry(self, entry: DictEntry, data: bytes):
        size, fmt, _ = TYPE_INFO[entry.type_code]
        if len(data) != size:
            raise ProtocolError(f"decode size mismatch for 0x{entry.address:04X}: expected {size}, got {len(data)}")
        return struct.unpack(fmt, data)[0]

    def _encode_by_entry(self, entry: DictEntry, value) -> bytes:
        _, fmt, _ = TYPE_INFO[entry.type_code]
        return struct.pack(fmt, value)

    def _mutate_value(self, entry: DictEntry, old_value):
        type_code = entry.type_code

        if type_code == TYPE_FLOAT32:
            if not isinstance(old_value, float) or not math.isfinite(old_value):
                return 1.0
            return old_value + 0.5

        if type_code == TYPE_UINT8:
            return (int(old_value) + 1) & 0xFF
        if type_code == TYPE_INT8:
            return int(old_value) - 1 if int(old_value) >= 127 else int(old_value) + 1
        if type_code == TYPE_UINT16:
            return (int(old_value) + 1) & 0xFFFF
        if type_code == TYPE_INT16:
            return int(old_value) - 1 if int(old_value) >= 32767 else int(old_value) + 1
        if type_code == TYPE_UINT32:
            return (int(old_value) + 1) & 0xFFFFFFFF
        if type_code == TYPE_INT32:
            return int(old_value) - 1 if int(old_value) >= 2147483647 else int(old_value) + 1
        if type_code == TYPE_UINT64:
            return (int(old_value) + 1) & 0xFFFFFFFFFFFFFFFF

        raise ProtocolError(f"unsupported mutate type {type_code}")

    def _read_values(self, addresses: List[int], timeout_s: float) -> Dict[int, bytes]:
        if not addresses:
            raise ProtocolError("_read_values called with empty address list")

        expected_len = 0
        for addr in addresses:
            entry = self.dict_by_addr.get(addr)
            if entry is None:
                raise ProtocolError(f"address 0x{addr:04X} not in dictionary")
            expected_len += entry.size

        self.client.send_frame(CMD_READ, self._build_read_payload(addresses))
        frame = self._expect_frame((CMD_READ_ACK,), timeout_s)

        if len(frame.payload) != expected_len:
            raise ProtocolError(f"Read-Data payload length mismatch: expected {expected_len}, got {len(frame.payload)}")

        out: Dict[int, bytes] = {}
        offset = 0
        for addr in addresses:
            size = self.dict_by_addr[addr].size
            out[addr] = frame.payload[offset:offset + size]
            offset += size

        return out

    def _write_values(self, pairs: List[Tuple[int, bytes]], timeout_s: float) -> Tuple[int, int]:
        payload = bytearray()
        for addr, raw in pairs:
            payload.extend(struct.pack("<H", addr))
            payload.extend(raw)

        self.client.send_frame(CMD_WRITE, bytes(payload))
        frame = self._expect_frame((CMD_WRITE_ACK,), timeout_s)

        if len(frame.payload) != 3:
            raise ProtocolError(f"Write-Ack payload length must be 3, got {len(frame.payload)}")

        first_addr = frame.payload[0] | (frame.payload[1] << 8)
        count = frame.payload[2]
        return first_addr, count

    def test_read(self) -> str:
        if not self.dictionary:
            raise ProtocolError("dictionary is empty, run dictionary test first")

        selected = [e.address for e in self.dictionary[: min(6, len(self.dictionary))]]
        values = self._read_values(selected, self.args.tcp_timeout)

        pretty = []
        for addr in selected:
            entry = self.dict_by_addr[addr]
            decoded = self._decode_by_entry(entry, values[addr])
            pretty.append(f"0x{addr:04X} {entry.name}={decoded}")

        return "read successful: " + "; ".join(pretty)

    def test_write_live(self) -> str:
        live = next((e for e in self.dictionary if e.access == ACCESS_WRITE_LIVE), None)
        if live is None:
            raise ProtocolError("no write-live parameter in dictionary")

        before = self._read_values([live.address], self.args.tcp_timeout)[live.address]
        before_decoded = self._decode_by_entry(live, before)

        mutated = self._mutate_value(live, before_decoded)
        mutated_raw = self._encode_by_entry(live, mutated)

        first_addr, count = self._write_values([(live.address, mutated_raw)], self.args.tcp_timeout)
        if first_addr != live.address or count != 1:
            raise ProtocolError(f"Write-Ack mismatch: first=0x{first_addr:04X}, count={count}")

        after = self._read_values([live.address], self.args.tcp_timeout)[live.address]
        after_decoded = self._decode_by_entry(live, after)

        if after != mutated_raw:
            raise ProtocolError(
                f"write-live verification mismatch on 0x{live.address:04X}: expected {mutated}, got {after_decoded}"
            )

        restore_first, restore_count = self._write_values([(live.address, before)], self.args.tcp_timeout)
        if restore_first != live.address or restore_count != 1:
            raise ProtocolError(f"restore Write-Ack mismatch: first=0x{restore_first:04X}, count={restore_count}")

        restored = self._read_values([live.address], self.args.tcp_timeout)[live.address]
        if restored != before:
            raise ProtocolError(f"restore verification mismatch on 0x{live.address:04X}")

        return (
            f"write-live ok on 0x{live.address:04X} ({live.name}): "
            f"{before_decoded} -> {after_decoded} -> restored"
        )

    def test_write_denied(self) -> str:
        ro = next((e for e in self.dictionary if e.access == ACCESS_READ_ONLY), None)
        if ro is None:
            raise ProtocolError("no read-only parameter in dictionary")

        zero_raw = bytes([0] * ro.size)
        payload = struct.pack("<H", ro.address) + zero_raw
        self.client.send_frame(CMD_WRITE, payload)
        address, code = self._expect_error_frame(self.args.tcp_timeout)

        if code != ERR_ACCESS_DENIED:
            raise ProtocolError(f"expected Access Denied (0x02), got 0x{code:02X}")
        if address != ro.address:
            raise ProtocolError(f"expected error address 0x{ro.address:04X}, got 0x{address:04X}")

        return f"write denied correctly for read-only address 0x{ro.address:04X} ({ro.name})"

    def test_invalid_address_errors(self) -> str:
        used = {e.address for e in self.dictionary}
        candidate = (max(used) + 1) & 0xFFFF
        while candidate in used:
            candidate = (candidate + 1) & 0xFFFF

        self.client.send_frame(CMD_READ, struct.pack("<H", candidate))
        r_addr, r_code = self._expect_error_frame(self.args.tcp_timeout)
        if r_code != ERR_INVALID_ADDR or r_addr != candidate:
            raise ProtocolError(f"invalid read expected addr=0x{candidate:04X},code=0x01 got addr=0x{r_addr:04X},code=0x{r_code:02X}")

        self.client.send_frame(CMD_WRITE, struct.pack("<H", candidate))
        w_addr, w_code = self._expect_error_frame(self.args.tcp_timeout)
        if w_code != ERR_INVALID_ADDR or w_addr != candidate:
            raise ProtocolError(f"invalid write expected addr=0x{candidate:04X},code=0x01 got addr=0x{w_addr:04X},code=0x{w_code:02X}")

        return f"invalid address handling ok for 0x{candidate:04X} (read/write)"

    def test_config_write_and_commit(self) -> str:
        cfg = next((e for e in self.dictionary if e.access == ACCESS_WRITE_AFTER_RESTART), None)
        if cfg is None:
            raise ProtocolError("no write-after-restart parameter in dictionary")

        before = self._read_values([cfg.address], self.args.tcp_timeout)[cfg.address]
        before_decoded = self._decode_by_entry(cfg, before)

        mutated = self._mutate_value(cfg, before_decoded)
        mutated_raw = self._encode_by_entry(cfg, mutated)

        first_addr, count = self._write_values([(cfg.address, mutated_raw)], self.args.tcp_timeout)
        if first_addr != cfg.address or count != 1:
            raise ProtocolError(f"config Write-Ack mismatch: first=0x{first_addr:04X}, count={count}")

        self.client.send_frame(CMD_CONFIG_COMMIT, b"")
        commit_ack = self._expect_frame((CMD_CONFIG_COMMIT_ACK,), self.args.tcp_timeout)
        if len(commit_ack.payload) != 0:
            raise ProtocolError(f"Update-Config-Ack payload must be empty, got {len(commit_ack.payload)}")

        restore_first, restore_count = self._write_values([(cfg.address, before)], self.args.tcp_timeout)
        if restore_first != cfg.address or restore_count != 1:
            raise ProtocolError(f"config restore Write-Ack mismatch: first=0x{restore_first:04X}, count={restore_count}")

        self.client.send_frame(CMD_CONFIG_COMMIT, b"")
        restore_ack = self._expect_frame((CMD_CONFIG_COMMIT_ACK,), self.args.tcp_timeout)
        if len(restore_ack.payload) != 0:
            raise ProtocolError("restore Update-Config-Ack payload must be empty")

        return (
            f"config write+commit ack ok on 0x{cfg.address:04X} ({cfg.name}); "
            f"value staged to {mutated} and restored"
        )

    def _open_udp_listener(self) -> socket.socket:
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except OSError:
            pass
        udp.bind(("", self.args.udp_port))
        udp.settimeout(self.args.udp_timeout)
        self.logger.info("UDP listener bound to 0.0.0.0:%d", self.args.udp_port)
        return udp

    def _parse_stream_udp(self, packet: bytes, expected_id: int, entries: List[DictEntry]) -> Tuple[int, int, int, Dict[int, object]]:
        if len(packet) < 14:
            raise ProtocolError(f"UDP stream packet too short: {len(packet)}")
        if packet[0:2] != START_BYTES:
            raise ProtocolError("UDP stream packet missing start bytes")

        stream_id = packet[2]
        seq = packet[3] | (packet[4] << 8)
        ts = int.from_bytes(packet[5:13], "little", signed=False)
        count = packet[13]

        if stream_id != expected_id:
            raise ProtocolError(f"UDP stream id mismatch: expected {expected_id}, got {stream_id}")
        if count != len(entries):
            raise ProtocolError(f"UDP count mismatch: expected {len(entries)}, got {count}")

        expected_data_len = sum(e.size for e in entries)
        data = packet[14:]
        if len(data) != expected_data_len:
            raise ProtocolError(f"UDP data length mismatch: expected {expected_data_len}, got {len(data)}")

        decoded: Dict[int, object] = {}
        offset = 0
        for entry in entries:
            part = data[offset:offset + entry.size]
            decoded[entry.address] = self._decode_by_entry(entry, part)
            offset += entry.size

        return stream_id, seq, ts, decoded

    def test_stream_flow(self) -> str:
        stream_candidates = self.dictionary[: min(3, len(self.dictionary))]
        if not stream_candidates:
            raise ProtocolError("dictionary empty, cannot run stream test")

        stream_id = self.args.stream_id
        freq_x100 = self.args.stream_freq_x100
        regs = [e.address for e in stream_candidates]

        udp = self._open_udp_listener()
        try:
            payload = bytes([stream_id]) + struct.pack("<H", freq_x100) + b"".join(struct.pack("<H", a) for a in regs)
            self.client.send_frame(CMD_STREAM_START, payload)

            ack = self._expect_frame((CMD_STREAM_ACK,), self.args.tcp_timeout)
            if len(ack.payload) < 2:
                raise ProtocolError("Stream-Ack payload too short")
            ack_id = ack.payload[0]
            ack_count = ack.payload[1]
            ack_addrs_raw = ack.payload[2:]
            if ack_id != stream_id:
                raise ProtocolError(f"Stream-Ack stream id mismatch: expected {stream_id}, got {ack_id}")
            if ack_count == 0:
                raise ProtocolError("Stream-Ack count is zero")
            if len(ack_addrs_raw) != ack_count * 2:
                raise ProtocolError(
                    f"Stream-Ack address bytes mismatch: expected {ack_count * 2}, got {len(ack_addrs_raw)}"
                )

            ack_addrs = [ack_addrs_raw[i] | (ack_addrs_raw[i + 1] << 8) for i in range(0, len(ack_addrs_raw), 2)]
            ack_entries = [self.dict_by_addr[a] for a in ack_addrs if a in self.dict_by_addr]
            if len(ack_entries) != len(ack_addrs):
                raise ProtocolError("Stream-Ack contained address not present in dictionary")

            packets = []
            deadline = now_s() + self.args.stream_wait_s
            while len(packets) < self.args.stream_packets and now_s() < deadline:
                try:
                    pkt, sender = udp.recvfrom(2048)
                except socket.timeout:
                    continue

                self.logger.debug("UDP RX from %s:%d len=%d raw=%s", sender[0], sender[1], len(pkt), hex_bytes(pkt))
                parsed = self._parse_stream_udp(pkt, stream_id, ack_entries)
                packets.append(parsed)

                sid, seq, ts, decoded = parsed
                self.logger.info(
                    "UDP packet %d/%d sid=%d seq=%d ts=%d decoded=%s",
                    len(packets),
                    self.args.stream_packets,
                    sid,
                    seq,
                    ts,
                    {f"0x{k:04X}": v for k, v in decoded.items()},
                )

            if len(packets) < self.args.stream_packets:
                raise ProtocolError(
                    f"received only {len(packets)} UDP packets, expected {self.args.stream_packets}"
                )

            self.client.send_frame(CMD_STREAM_STOP, bytes([stream_id]))
            stop_ack = self._expect_frame((CMD_STREAM_STOP_ACK,), self.args.tcp_timeout)
            if len(stop_ack.payload) != 1 or stop_ack.payload[0] != stream_id:
                raise ProtocolError("Stream-Stop-Ack payload mismatch")

            seqs = [p[1] for p in packets]
            return (
                f"stream start/udp/stop ok: ack_count={ack_count}, "
                f"packets={len(packets)}, seq_first={seqs[0]}, seq_last={seqs[-1]}"
            )
        finally:
            udp.close()

    def test_stream_stop_inactive(self) -> str:
        stream_id = self.args.stream_id
        self.client.send_frame(CMD_STREAM_STOP, bytes([stream_id]))
        address, code = self._expect_error_frame(self.args.tcp_timeout)

        if address != 0:
            raise ProtocolError(f"expected stream-stop inactive error address 0x0000, got 0x{address:04X}")
        if code != ERR_OTHER:
            raise ProtocolError(f"expected stream-stop inactive error code 0x03, got 0x{code:02X}")

        return f"inactive stream stop correctly returned Error-Resp 0x03 for id={stream_id}"

    def _print_summary(self) -> int:
        self.logger.info("==== Testsuite Summary ====")
        passed = sum(1 for r in self.results if r.passed)
        failed = len(self.results) - passed

        for r in self.results:
            status = "PASS" if r.passed else "FAIL"
            self.logger.info("%s | %s | %.3fs | %s", status, r.name, r.duration_s, r.details)

        self.logger.info("TOTAL: %d, PASSED: %d, FAILED: %d", len(self.results), passed, failed)

        if failed > 0:
            self.logger.error("At least one test failed. See log for TX/RX and diagnostics.")
            return 1

        self.logger.info("All tests passed.")
        return 0


def setup_logger(log_file: str, verbose: bool) -> logging.Logger:
    logger = logging.getLogger("inverter-testsuite")
    logger.setLevel(logging.DEBUG)
    logger.handlers.clear()

    formatter = logging.Formatter("%(asctime)s | %(levelname)-7s | %(message)s")

    ch = logging.StreamHandler(sys.stdout)
    ch.setLevel(logging.DEBUG if verbose else logging.INFO)
    ch.setFormatter(formatter)

    fh = logging.FileHandler(log_file, mode="w", encoding="utf-8")
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(formatter)

    logger.addHandler(ch)
    logger.addHandler(fh)
    return logger


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run full Ethernet protocol tests against inverter firmware",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--host", required=True, help="Inverter IPv4 address")
    parser.add_argument("--tcp-port", type=int, default=3030, help="TCP control port")
    parser.add_argument("--udp-port", type=int, default=UDP_STREAM_PORT, help="UDP stream listen port")
    parser.add_argument("--tcp-timeout", type=float, default=2.0, help="TCP frame timeout in seconds")
    parser.add_argument("--udp-timeout", type=float, default=0.5, help="UDP receive timeout in seconds")
    parser.add_argument("--dict-total-timeout", type=float, default=3.0, help="Max dictionary receive time in seconds")
    parser.add_argument("--dict-silence-timeout", type=float, default=0.3, help="Dictionary completion silence timeout in seconds")
    parser.add_argument("--stream-id", type=int, default=1, help="Stream ID used by stream tests")
    parser.add_argument("--stream-freq-x100", type=int, default=1000, help="Stream frequency in x100 Hz")
    parser.add_argument("--stream-packets", type=int, default=3, help="Number of UDP packets required for pass")
    parser.add_argument("--stream-wait-s", type=float, default=4.0, help="Max wait time for required UDP packets")
    parser.add_argument("--log-file", default="inverter_testsuite.log", help="Detailed log file path")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose console debug logs")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.stream_id < 0 or args.stream_id > 255:
        print("Invalid --stream-id, must be in [0..255]", file=sys.stderr)
        return 2
    if args.stream_freq_x100 <= 0:
        print("Invalid --stream-freq-x100, must be > 0", file=sys.stderr)
        return 2

    logger = setup_logger(args.log_file, args.verbose)

    suite = InverterTestSuite(args, logger)
    return suite.run()


if __name__ == "__main__":
    sys.exit(main())
