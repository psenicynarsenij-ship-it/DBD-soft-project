"""Package an existing Windows build; does not build or claim native verification."""
import argparse
import pathlib
import subprocess
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("build", type=pathlib.Path, help="directory containing Windows exe and matching memory.fixture.json")
args = parser.parse_args()
build = args.build.resolve()
out = root / "downloads"
out.mkdir(exist_ok=True)

def archive(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for source, name in entries:
            z.write(source, name)
    with zipfile.ZipFile(path) as z:
        if z.testzip() is not None:
            raise RuntimeError("ZIP CRC verification failed")
        for source, name in entries:
            if z.read(name) != source.read_bytes():
                raise RuntimeError(f"ZIP content mismatch: {name}")
    print(f"{path.name}: {len(entries)} files, {path.stat().st_size} bytes; verified")

tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
sources = [(root / name, "VisualESPOverlay/" + name) for name in tracked
           if name and not name.startswith("downloads/") and (root / name).is_file()]
archive(out / "VisualESPOverlay-source.zip", sources)
exe = ["VisualESPOverlay.exe", "frame_tool.exe", "EspMemoryFixture.exe", "fixture_layout_tool.exe", "memory.fixture.json"]
files = [(build / name, name) for name in exe]
for name in ["README.md", "VALIDATION.md", "offsets.json", "config.example.json"]:
    files.append((root / name, name))
files.extend((root / name, name) for name in ["examples/frame.json", "examples/config.memory-fixture.json"])
files.extend((p, p.relative_to(root).as_posix()) for p in sorted((root / "docs").rglob("*")) if p.is_file())
files.extend((build / name, name) for name in ["LICENSE-imgui.txt", "LICENSE-json.txt"])
archive(out / "VisualESPOverlay-demo-win64.zip", files)
