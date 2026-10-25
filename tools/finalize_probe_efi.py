"""Finalize the MSVC-linked freestanding probe as an EFI application."""
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
pe = struct.unpack_from("<I", data, 0x3c)[0]
if data[:2] != b"MZ" or data[pe:pe+4] != b"PE\0\0":
    raise ValueError("Not a PE image")
optional = pe + 24
if struct.unpack_from("<H", data, optional)[0] != 0x20b:
    raise ValueError("Not PE32+")
if struct.unpack_from("<H", data, optional + 68)[0] != 1:
    raise ValueError("Expected freshly linked native subsystem")
struct.pack_into("<H", data, optional + 68, 10)
# Clear the linker checksum because the subsystem field changed.
struct.pack_into("<I", data, optional + 64, 0)
path.write_bytes(data)
