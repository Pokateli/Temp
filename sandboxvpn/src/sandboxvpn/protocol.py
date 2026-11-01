"""Binary framing; 9 bytes per frame, no Base64 overhead."""
import enum
import struct

MAX_PAYLOAD = 65507
HEADER = struct.Struct('!BII')

class Kind(enum.IntEnum):
    TCP_OPEN = 1
    TCP_OK = 2
    TCP_ERROR = 3
    TCP_DATA = 4
    TCP_CLOSE = 5
    UDP_DATA = 6
    UDP_REPLY = 7


def pack(kind: Kind, channel: int, payload: bytes = b'') -> bytes:
    if not isinstance(payload, bytes) or len(payload) > MAX_PAYLOAD:
        raise ValueError('invalid payload')
    if not 0 < channel < 2**32:
        raise ValueError('invalid channel')
    return HEADER.pack(int(kind), channel, len(payload)) + payload


def unpack(buf: bytes) -> tuple[Kind, int, bytes]:
    if not isinstance(buf, bytes) or len(buf) < HEADER.size:
        raise ValueError('invalid binary header')
    kind, channel, size = HEADER.unpack_from(buf)
    if channel == 0 or size > MAX_PAYLOAD or len(buf) != size + HEADER.size:
        raise ValueError('invalid packet size/channel')
    return Kind(kind), channel, buf[HEADER.size:]

