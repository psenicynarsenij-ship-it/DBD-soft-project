"""Read-only BMP diagnostics for a captured test artifact (standard library only)."""
import json
import pathlib
import struct

path = pathlib.Path('build/native-windows/memory-before.bmp')
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
