from __future__ import annotations

import argparse
import hashlib
import struct
import zipfile
from pathlib import Path

DLL_NAME = "iliashdz.bismuth.dll"
IMAGE_BASE = 0x180000000

# These RVAs and expected bytes identify the exact supplied DLL build.
CALL_SITE_RVA = 0x2E5D2
CODE_CAVE_RVA = 0x5CE6C1
OBJECT_BATCH_DRAW_RVA = 0x2C3A0
EXPECTED_CALL = bytes.fromhex("E8 C9 DD FF FF")
EXPECTED_CAVE = b"\xCC" * 15


def rva_to_offset(dll: bytearray, rva: int) -> int:
    pe_offset = struct.unpack_from("<I", dll, 0x3C)[0]
    if dll[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise RuntimeError("Invalid PE signature")

    section_count = struct.unpack_from("<H", dll, pe_offset + 6)[0]
    optional_header_size = struct.unpack_from("<H", dll, pe_offset + 20)[0]
    section_table = pe_offset + 24 + optional_header_size

    for index in range(section_count):
        section = section_table + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
            "<IIII", dll, section + 8
        )
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            return raw_pointer + rva - virtual_address

    raise RuntimeError(f"RVA {rva:#x} is not mapped to a PE section")


def patch_archive(input_path: Path, output_path: Path) -> None:
    with zipfile.ZipFile(input_path, "r") as archive:
        files = {
            info.filename: (info, archive.read(info.filename))
            for info in archive.infolist()
            if not info.is_dir()
        }

    if DLL_NAME not in files:
        raise RuntimeError(f"{DLL_NAME} is missing from the input .geode")

    dll = bytearray(files[DLL_NAME][1])
    call_offset = rva_to_offset(dll, CALL_SITE_RVA)
    cave_offset = rva_to_offset(dll, CODE_CAVE_RVA)

    if bytes(dll[call_offset:call_offset + len(EXPECTED_CALL)]) != EXPECTED_CALL:
        raise RuntimeError("Call site does not match the expected DLL revision")
    if bytes(dll[cave_offset:cave_offset + len(EXPECTED_CAVE)]) != EXPECTED_CAVE:
        raise RuntimeError("Selected code cave is not the expected unused padding")

    # Skip the invalid low ObjectBatch pointer produced by a null Renderer.
    # For valid pointers, tail-jump to the original ObjectBatch::draw.
    stub_va = IMAGE_BASE + CODE_CAVE_RVA
    stub = bytearray.fromhex("48 81 F9 00 00 01 00 72 05 E9")
    jump_next_va = stub_va + len(stub) + 4
    jump_target_va = IMAGE_BASE + OBJECT_BATCH_DRAW_RVA
    stub += struct.pack("<i", jump_target_va - jump_next_va)
    stub += b"\xC3"

    if len(stub) != 15:
        raise RuntimeError("Unexpected trampoline size")

    call_va = IMAGE_BASE + CALL_SITE_RVA
    call_next_va = call_va + 5
    new_call = b"\xE8" + struct.pack("<i", stub_va - call_next_va)
    dll[call_offset:call_offset + 5] = new_call
    dll[cave_offset:cave_offset + len(stub)] = stub

    files[DLL_NAME] = (files[DLL_NAME][0], bytes(dll))
    # The old PDB no longer matches the patched DLL and is not needed at runtime.
    files.pop("iliashdz.bismuth.pdb", None)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, (info, data) in files.items():
            archive.writestr(info, data)

    with zipfile.ZipFile(output_path, "r") as archive:
        corrupt_member = archive.testzip()
        if corrupt_member:
            raise RuntimeError(f"Corrupt archive member: {corrupt_member}")

    print(f"Output: {output_path}")
    print(f"Size: {output_path.stat().st_size:,} bytes")
    print(f"SHA-256: {hashlib.sha256(output_path.read_bytes()).hexdigest()}")
    print(f"Patched DLL SHA-256: {hashlib.sha256(dll).hexdigest()}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Patch the exact supplied Bismuth Windows .geode build."
    )
    parser.add_argument("--input", type=Path, default=Path("iliashdz.bismuth.geode"))
    parser.add_argument(
        "--output", type=Path, default=Path("bismuth-null-renderer-hotfix.geode")
    )
    args = parser.parse_args()
    patch_archive(args.input, args.output)


if __name__ == "__main__":
    main()
