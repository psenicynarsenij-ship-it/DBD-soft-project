"""Read-only BMP diagnostics for a captured test artifact (standard library only)."""
import json
import pathlib
import struct

paths = [pathlib.Path('build/native-windows') / name for name in ['memory-before.bmp','memory-after-move.bmp','memory-after-camera.bmp']]
paths = [p for p in paths if p.exists()]
path = paths[-1]
print('::notice title=Captured stages::'+json.dumps([p.name for p in paths]))
b = path.read_bytes()
offset = struct.unpack_from('<I', b, 10)[0]
w, height = struct.unpack_from('<ii', b, 18)
h = abs(height)
points = []
opaque = 0
colored_no_alpha = 0
for y in range(h):
    for x in range(w):
        p = offset + ((y if height < 0 else h-1-y)*w+x)*4
        blue, green, red, alpha = b[p:p+4]
        opaque += alpha > 0
        if blue > 120 and green > 120 and red < 100:
            colored_no_alpha += 1
            if alpha:
                points.append((x, y))
report = {'width': w, 'height': height, 'opaque_pixels': opaque, 'cyan_no_alpha': colored_no_alpha,
          'cyan_count': len(points), 'expected_survivor_center': [w*0.44375, h*0.5],
          'cyan_bounds': [min(x for x,y in points),min(y for x,y in points),max(x for x,y in points),max(y for x,y in points)] if points else None,
          'cyan_samples': points[:12]}
print('::notice title=Captured pixel diagnostics::'+json.dumps(report))
# A small lossless crop, encoded in one check annotation for restricted log clients.
import base64
import zlib
cx, cy = (min(x for x,y in points)+max(x for x,y in points))//2, round(h*0.5166666667)
crop_w, crop_h = 64, 64
raw = bytearray()
for y in range(cy-32, cy+32):
    raw.append(0)
    for x in range(cx-32, cx+32):
        p = offset + ((y if height < 0 else h-1-y)*w+x)*4
        blue,green,red,alpha = b[p:p+4]
        raw.extend((red,green,blue,alpha))
def chunk(kind, data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',64,64,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
print('::notice title=Captured crop PNG::'+base64.b64encode(png).decode())
