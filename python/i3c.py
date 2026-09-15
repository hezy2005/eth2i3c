'''
Usage:

from i3c import *
c = I3CController()
c.target_negotiate_i3c_by_sa(0x50)
c.targets
c.rstdaa()
c.entdaa()
c.targets
c.target_bind(0x50, 0x10)
c.getbcr()
c.getdcr()
c.w(0x1, [0x10, 0x20, 0x30, 0x40])
c.r(0x1, 4)
c.r_current(4)
c.h(0, 128, print_addr_ahead=True)
c.r(3)
c.w(26, 0x08)
c.target_set_active_by_da(0x10)
c.target_create_by_sa(0x50)
c.target_list()
c.target_set_active_by_sa(0x50)
c.target_set_active_by_da(0x10)
c.target_bind(0x50, 0x10) # Bind the SA-only and DA-only entries. The merged entry prefers I3C.
c.target_set_protocol_by_sa(0x50, "i2c")
c.target_set_protocol_by_da(0x10, "i3c")

'''
import argparse
from dataclasses import dataclass, field
import secrets
import socket
import sys
from typing import Iterable, List, Optional, Sequence, Tuple, Union

HOST = "192.168.1.212"
PORT = 1000
DEFAULT_TIMEOUT = 5.0 # seconds

class I3CError(RuntimeError):
    pass


@dataclass
class I3CTarget:
    sa: int
    da: int
    protocol: str
    is_active: bool
    payload: int
    pid: int = field(init=False)
    bcr: int = field(init=False)
    dcr: int = field(init=False)

    def __post_init__(self) -> None:
        self.payload &= 0xFFFFFFFFFFFFFFFF
        raw_pid = self.payload & 0xFFFFFFFFFFFF
        self.pid = int.from_bytes(raw_pid.to_bytes(6, "little"), "big")
        self.bcr = (self.payload >> 48) & 0xFF
        self.dcr = (self.payload >> 56) & 0xFF


class I3CController:
    def __init__(
        self,
        host: str = HOST,
        port: int = PORT,
        timeout: float = DEFAULT_TIMEOUT,
    ):
        if not host:
            raise ValueError("host must not be empty")
        if not 1 <= port <= 65535:
            raise ValueError("port must be between 1 and 65535")
        if timeout <= 0:
            raise ValueError("timeout must be greater than zero")
        self.host = host
        self.port = port
        self.timeout = timeout
        self._sock: Optional[socket.socket] = None
        self._targets: List[I3CTarget] = []
        self.connect()

    def h(
        self,
        start_addr: int,
        length: int = 1,
        step: int = 1,
        width: int = 0x10,
        print_addr_ahead: bool = False,
    ) -> None:
        if length < 0 or step <= 0 or width <= 0:
            raise ValueError("length must be non-negative; step and width must be positive")
        if step == 1:
            values = [self.r(start_addr)] if length == 1 else self.r(start_addr, length)
        else:
            values = [self.r(start_addr + index * step) for index in range(length)]
        for row_start in range(0, length, width):
            if print_addr_ahead:
                print("{:02x}: ".format(start_addr + row_start * step), end="", flush=True)
            print(" ".join("{:02x}".format(value) for value in values[row_start:row_start + width]))

    def r(self, start_addr: int, length: int = 1):
        data = self._raw_cmd("*twi", length, start_addr)
        values = self._parse_byte_tokens(data)
        return values[0] if length == 1 else values

    def r_current(self, length: int) -> List[int]:
        data = self._raw_cmd("*twi", length)
        return self._parse_byte_tokens(data)

    def w(self, start_addr: int, value: Union[int, str, bytes, bytearray, Sequence[int]]) -> None:
        if isinstance(value, str):
            payload = value.encode("ascii")
        elif isinstance(value, (bytes, bytearray)):
            payload = bytes(value)
        elif isinstance(value, Sequence):
            payload = bytes(int(item) & 0xFF for item in value)
        else:
            payload = bytes([int(value) & 0xFF])
        self._raw_cmd("*twi", len(payload), start_addr, payload)

    def close(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            finally:
                self._sock = None

    def __enter__(self) -> "I3CController":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def connect(self) -> socket.socket:
        if self._sock is None:
            self._sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
            try:
                self._sock.settimeout(self.timeout)
                self._sock.recv(4096) # receive welcome message
            except BaseException:
                self.close()
                raise
        return self._sock

    def _request(self, command: str) -> str:
        payload = (command + "\n").encode("ascii")
        last_error: Optional[BaseException] = None

        for _ in range(1):
            try:
                sock = self.connect()
                sock.sendall(payload)
                chunks: List[bytes] = []
                while True:
                    try:
                        data = sock.recv(4096)
                    except socket.timeout:
                        break
                    if not data:
                        raise ConnectionError("connection closed")
                    chunks.append(data)
                    if b"\n" in data:
                        break
                if not chunks:
                    raise I3CError(f"no response for {command}")
                text = b"".join(chunks).decode(errors="ignore")
                lines = [line.strip() for line in text.splitlines() if line.strip() and line.strip() != ">"]
                if not lines:
                    raise I3CError(f"no response for {command}")
                return lines[0]
            except (OSError, ConnectionError) as exc:
                last_error = exc
                self.close()

        raise I3CError(f"request failed for {command}: {last_error}")

    @staticmethod
    def _parse_response(resp: str) -> Tuple[int, Optional[str], List[str]]:
        if not resp.startswith("$") or ";<>" not in resp:
            raise I3CError(f"invalid response: {resp}")
        head, tail = resp.split(";<>", 1)
        code = int(head[1:], 16)
        tail = tail.strip()
        if code != 0:
            return code, tail or None, []
        if tail in ("", ">"):
            return code, None, []
        parts = tail.split() if tail else []
        if parts and parts[0].startswith("$"):
            parts[0] = parts[0][1:]
        return code, None, parts

    @staticmethod
    def _parse_byte_tokens(tokens: Sequence[str]) -> List[int]:
        def parse_byte(piece: str) -> int:
            piece = piece.strip()
            if not piece:
                raise ValueError("empty byte token")
            if piece.lower().startswith("0x"):
                return int(piece, 16) & 0xFF
            return int(piece, 16) & 0xFF

        values: List[int] = []
        for token in tokens:
            for piece in token.replace(",", " ").split():
                if not piece:
                    continue
                values.append(parse_byte(piece))
        # return bytes(values)
        return values

    def _request_ok(self, command: str) -> List[str]:
        resp = self._request(command)
        code, err, data = self._parse_response(resp)
        if code != 0:
            raise I3CError(err or f"command failed: {resp}")
        return data

    @staticmethod
    def _fmt_params(*values: Union[int, str, bytes, bytearray, Sequence[int]]) -> str:
        parts: List[str] = []
        for value in values:
            if isinstance(value, int):
                parts.append(f"0x{value:X}")
            elif isinstance(value, (bytes, bytearray)):
                parts.extend(f"0x{b:02X}" for b in value)
            elif isinstance(value, str):
                parts.append(value)
            else:
                parts.extend(f"0x{int(v) & 0xFF:02X}" for v in value)
        return " ".join(parts)

    def _cmd(self, name: str, *params: Union[int, str, bytes, bytearray, Sequence[int]]) -> List[str]:
        command = f"*I3C_{name}"
        param_str = self._fmt_params(*params)
        if param_str:
            command += " " + param_str
        return self._request_ok(command)

    def _raw_cmd(self, command: str, *params: Union[int, str, bytes, bytearray, Sequence[int]]) -> List[str]:
        param_str = self._fmt_params(*params)
        if param_str:
            command += " " + param_str
        return self._request_ok(command)

    def rstdaa(self) -> None:
        self._cmd("RSTDAA")
        self._targets = []

    def _parse_targets(self, data: List[str]) -> List[I3CTarget]:
        if not data:
            return []
        count = int(data[0], 0)
        items = data[1:]
        targets: List[I3CTarget] = []
        for i in range(count):
            base = i * 5
            if base + 4 >= len(items):
                raise I3CError("TARGET_LIST returned incomplete data")
            access_by_i3c = bool(int(items[base + 2], 0))
            targets.append(
                I3CTarget(
                    sa=int(items[base], 0),
                    da=int(items[base + 1], 0),
                    protocol="i3c" if access_by_i3c else "i2c",
                    is_active=bool(int(items[base + 3], 0)),
                    payload=int(items[base + 4], 0),
                )
            )
        self._targets = targets
        return targets

    def entdaa(self) -> List[I3CTarget]:
        self._cmd("ENTDAA")
        return self.target_list()

    @property
    def targets(self) -> List[I3CTarget]:
        return self.target_list()

    def target_list(self) -> List[I3CTarget]:
        return self._parse_targets(self._cmd("TARGET_LIST"))

    def target_create_by_sa(self, sa: int) -> None:
        self._cmd("TARGET_CREATE_BY_SA", sa)

    def target_set_active_by_sa(self, sa: int) -> None:
        self._cmd("TARGET_SET_ACTIVE_BY_SA", sa)

    def target_set_active_by_da(self, da: int) -> None:
        self._cmd("TARGET_SET_ACTIVE_BY_DA", da)

    def target_bind(self, sa: int, da: int) -> None:
        self._cmd("TARGET_BIND", sa, da)

    def target_negotiate_i3c_by_sa(self, sa: int) -> I3CTarget:
        data = self._cmd("TARGET_NEGOTIATE_I3C_BY_SA", sa)
        if len(data) != 1:
            raise I3CError(f"TARGET_NEGOTIATE_I3C_BY_SA expected 1 DA, got {len(data)}")
        da = int(data[0], 0)
        for target in self.target_list():
            if target.sa == sa and target.da == da:
                return target
        raise I3CError(f"negotiated target SA 0x{sa:02X}, DA 0x{da:02X} not found")

    @staticmethod
    def _protocol_value(protocol: str) -> int:
        normalized = protocol.strip().lower()
        if normalized not in ("i2c", "i3c"):
            raise ValueError("protocol must be 'i2c' or 'i3c'")
        return 1 if normalized == "i3c" else 0

    def target_set_protocol_by_sa(self, sa: int, protocol: str) -> None:
        self._cmd("TARGET_SET_PROTOCOL_BY_SA", sa, self._protocol_value(protocol))

    def target_set_protocol_by_da(self, da: int, protocol: str) -> None:
        self._cmd("TARGET_SET_PROTOCOL_BY_DA", da, self._protocol_value(protocol))

    def getpid(self) -> int:
        data = self._cmd("GETPID")
        if len(data) != 6:
            raise I3CError(f"GETPID expected 6 bytes, got {len(data)}")
        pid_bytes = bytes(int(token, 0) & 0xFF for token in data)
        return int.from_bytes(pid_bytes, byteorder="big")

    def getbcr(self) -> int:
        data = self._cmd("GETBCR")
        if not data:
            raise I3CError("GETBCR returned no data")
        return int(data[0], 0)

    def getdcr(self) -> int:
        data = self._cmd("GETDCR")
        if not data:
            raise I3CError("GETDCR returned no data")
        return int(data[0], 0)

    def getmwl(self) -> int:
        data = self._cmd("GETMWL")
        if len(data) < 2:
            raise I3CError("GETMWL returned incomplete data")
        return (int(data[0], 0) & 0xFF) << 8 | (int(data[1], 0) & 0xFF)

    def getmrl(self) -> int:
        data = self._cmd("GETMRL")
        if len(data) < 2:
            raise I3CError("GETMRL returned incomplete data")
        return (int(data[0], 0) & 0xFF) << 8 | (int(data[1], 0) & 0xFF)

    def getstatus(self) -> List[int]:
        data = self._cmd("GETSTATUS")
        return [int(x, 0) for x in data]

    def setmwl(self, mwl: int) -> None:
        self._cmd("SETMWL", (mwl >> 8) & 0xFF, mwl & 0xFF)

    def setmrl(self, mrl: int) -> None:
        self._cmd("SETMRL", (mrl >> 8) & 0xFF, mrl & 0xFF)

    def enec(self, mask: int) -> None:
        self._cmd("ENEC", mask & 0xFF)

    def disec(self, mask: int) -> None:
        self._cmd("DISEC", mask & 0xFF)

def _hex_bytes(values: Iterable[int]) -> str:
    return " ".join(f"0x{v:02X}" for v in values)


def main() -> int:
    parser = argparse.ArgumentParser(description="I3C protocol client and self-test")
    parser.add_argument("--host", default=HOST, help="controller IPv4 address or host name")
    parser.add_argument("--port", type=int, default=PORT, help="controller TCP port")
    parser.add_argument("--no-test", action="store_true", help="skip built-in test case")
    args = parser.parse_args()

    c = I3CController(host=args.host, port=args.port)

    if args.no_test:
        print("I3C controller ready")
        return 0

    print(f"connected to {c.host}:{c.port}")

    try:
        print("RSTDAA")
        c.rstdaa()
    except Exception as exc:
        print(f"RSTDAA failed: {exc}")

    try:
        targets = c.entdaa()
        print("ENTDAA ->")
        for target in targets:
            print(
                f"  SA=0x{target.sa:02X} DA=0x{target.da:02X} "
                f"protocol={target.protocol} is_active={target.is_active} "
                f"PID=0x{target.pid:012X} BCR=0x{target.bcr:02X} DCR=0x{target.dcr:02X} "
                f"payload=0x{target.payload:016X}"
            )
    except Exception as exc:
        print(f"ENTDAA failed: {exc}")
        targets = []

    if len(targets) == 0:
        targets = c.targets
        if targets:
            print("TARGET_LIST ->")
            for target in targets:
                print(
                    f"  SA=0x{target.sa:02X} DA=0x{target.da:02X} "
                    f"protocol={target.protocol} is_active={target.is_active} "
                    f"PID=0x{target.pid:012X} BCR=0x{target.bcr:02X} DCR=0x{target.dcr:02X} "
                    f"payload=0x{target.payload:016X}"
                )

    if len(targets) == 0:
        print("no target found on I3C bus")
        return 1

    i3c_target = next((target for target in targets if target.da != 0), None)
    if i3c_target is None:
        print("no target with a dynamic address found on I3C bus")
        return 1

    # c.target_bind(0x50, i3c_target.da)
    c.target_set_active_by_da(i3c_target.da)

    try:
        active_target = next((target for target in c.target_list() if target.is_active), None)
        if active_target is None:
            raise I3CError("active target not found after target_set_active_by_da")
        print("ACTIVE TARGET ->")
        print(
            f"  SA=0x{active_target.sa:02X} DA=0x{active_target.da:02X} "
            f"protocol={active_target.protocol} is_active={active_target.is_active} "
            f"PID=0x{active_target.pid:012X} BCR=0x{active_target.bcr:02X} "
            f"DCR=0x{active_target.dcr:02X} payload=0x{active_target.payload:016X}"
        )
        pid = c.getpid()
        print(f"GETPID -> 0x{pid:016X}")
        bcr = c.getbcr()
        print(f"GETBCR -> 0x{bcr:02X}")
        dcr = c.getdcr()
        print(f"GETDCR -> 0x{dcr:02X}")
        ori_mwl = c.getmwl()
        print(f"GETMWL -> 0x{ori_mwl:02X}")
        new_mrl = 0x0ff0
        c.setmrl(new_mrl)
        assert c.getmrl() == new_mrl, f"SETMRL failed: expected 0x{new_mrl:04X}, got 0x{c.getmrl():04X}"
        c.setmwl(ori_mwl)
        assert c.getmwl() == ori_mwl, f"SETMWL failed: expected 0x{ori_mwl:04X}, got 0x{c.getmwl():04X}"

        c.w(126, [0, 0xa0])
        assert c.r(126, 2) == [0, 0xa0]

        reg = 0x80
        write_data_0 = list(secrets.token_bytes(4))
        write_data_1 = list(secrets.token_bytes(4))

        print(f"WRITE reg=0x{reg:02X}: {_hex_bytes(write_data_0)}")
        c.w(reg, write_data_0)

        print(f"WRITE reg=0x{reg + len(write_data_0):02X}: {_hex_bytes(write_data_1)}")
        c.w(reg + len(write_data_0), write_data_1)

        print(f"READ reg=0x{reg:02X} len={len(write_data_0)}")
        read_data = c.r(reg, len(write_data_0))
        print(_hex_bytes(read_data))
        assert read_data == write_data_0, (
            f"READ mismatch: expected {_hex_bytes(write_data_0)}, got {_hex_bytes(read_data)}"
        )

        print(f"READ_CURRENT len={len(write_data_1)}")
        current_data = c.r_current(len(write_data_1))
        print(_hex_bytes(current_data))
        assert current_data == write_data_1, (
            f"READ_CURRENT mismatch: expected {_hex_bytes(write_data_1)}, got {_hex_bytes(current_data)}"
        )

        print("register access self-test passed")
    except Exception as exc:
        print(f"assert_failed: {exc}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
