"""Repack DOL sections with 32-byte file offsets, lengths and trailing padding.

Loaders may round section reads up to a cache line. elf2dol versions can emit
an unpadded last data section, leaving that read past EOF. Keep the entry point,
addresses, BSS and original section bytes unchanged.
"""
from pathlib import Path
import struct,sys

path=Path(sys.argv[1]);raw=path.read_bytes()
if len(raw)<256:raise ValueError('DOL header is truncated')
offsets=struct.unpack_from('>18I',raw,0)
addresses=struct.unpack_from('>18I',raw,0x48)
sizes=struct.unpack_from('>18I',raw,0x90)
out=bytearray(raw[:256]);intervals=[]
for i,(offset,address,size) in enumerate(zip(offsets,addresses,sizes)):
    if not size:continue
    if offset<256 or offset+size>len(raw):raise ValueError(f'Section {i} is truncated')
    aligned=(size+31)&~31
    for j,(other,other_size) in enumerate(zip(addresses,sizes)):
        if j!=i and other_size and address<other<address+aligned:
            raise ValueError(f'Padding section {i} would overlap section {j}')
    start=len(out);out.extend(raw[offset:offset+size]);out.extend(bytes(aligned-size))
    struct.pack_into('>I',out,i*4,start)
    struct.pack_into('>I',out,0x90+i*4,aligned)
    intervals.append((i,start,aligned))
path.write_bytes(out)
print(f'DOL aligned: {len(raw)} -> {len(out)} bytes; {len(intervals)} sections')
