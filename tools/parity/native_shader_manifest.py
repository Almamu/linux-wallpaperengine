#!/usr/bin/env python3
"""Inventory the shader variants the native application compiled for a wallpaper.

The native application caches every compiled program in
``<workshop item>/shaders/blobsSM40/*.dxs`` (SHDV container: one DXBC blob per
stage, each with reflection data). Reflection lists every declared constant and
whether the compiled code actually reads it, so a blob says which feature
branches (#if combos) native selected for a material, with no decompilation.

    python tools/parity/native_shader_manifest.py WORKSHOP_ITEM_DIR [--json OUT]

Standard library only. Read-only. A cache can contain variants from earlier
native runs with other settings; treat an entry as "native compiled this at
least once", then confirm on a fresh private copy before relying on it.
"""
import argparse
import collections
import json
import pathlib
import struct
import sys

STAGES = {0xFFFF: "PS", 0xFFFE: "VS", 0x4753: "GS"}
OPCODES = {13: "discard", 69: "sample", 70: "sample_c", 71: "sample_c_lz", 72: "sample_l", 73: "sample_d", 74: "sample_b"}
BIND_TYPES = {0: "cbuffer", 2: "texture", 3: "sampler"}


def _cstr(data, offset):
    return data[offset:data.index(b"\0", offset)].decode("latin1")


def _chunks(blob, start):
    if len(blob) - start < 32:
        raise ValueError("truncated DXBC header")
    length, count = struct.unpack_from("<2I", blob, start + 24)
    header = 32 + 4 * count
    if not header <= length <= len(blob) - start:
        raise ValueError("invalid DXBC container length or chunk table")
    out = {}
    for index in range(count):
        offset = struct.unpack_from("<I", blob, start + 32 + 4 * index)[0]
        if not header <= offset <= length - 8:
            raise ValueError("DXBC chunk offset is outside its container")
        tag, size = struct.unpack_from("<4sI", blob, start + offset)
        if size > length - offset - 8 or tag.decode("latin1") in out:
            raise ValueError("invalid or duplicate DXBC chunk")
        payload = start + offset + 8
        out[tag.decode("latin1")] = blob[payload:payload + size]
    return out, start + length


def _reflection(data):
    cb_count, cb_offset, bind_count, bind_offset, version = struct.unpack_from("<5I", data, 0)
    variable_stride = 40 if version & 0xFFFF >= 0x0500 else 24
    buffers = []
    for index in range(cb_count):
        name, var_count, var_offset, size = struct.unpack_from("<4I", data, cb_offset + 24 * index)
        variables = []
        for var in range(var_count):
            var_name, _start, var_size, flags = struct.unpack_from("<4I", data, var_offset + variable_stride * var)
            variables.append({"name": _cstr(data, var_name), "bytes": var_size, "used": bool(flags & 2)})
        buffers.append({"name": _cstr(data, name), "bytes": size, "variables": variables})
    bindings = []
    for index in range(bind_count):
        name, kind, _ret, _dim, _samples, slot = struct.unpack_from("<6I", data, bind_offset + 32 * index)
        bindings.append({"name": _cstr(data, name), "type": BIND_TYPES.get(kind, str(kind)), "slot": slot})
    return STAGES.get(version >> 16, hex(version >> 16)), buffers, bindings


def _signature(data):
    if not data:
        return []
    out = []
    for index in range(struct.unpack_from("<I", data, 0)[0]):
        name, semantic_index, _sv, _type, _register, mask = struct.unpack_from("<5IB", data, 8 + 24 * index)
        out.append(f"{_cstr(data, name)}{semantic_index}:{bin(mask).count('1')}")
    return out


def _opcodes(code):
    count = len(code) // 4
    if count < 2 or len(code) % 4:
        raise ValueError("invalid DXBC shader token stream")
    tokens = struct.unpack(f"<{count}I", code)
    if tokens[1] != count:
        raise ValueError("DXBC shader token count differs from its chunk size")
    seen = collections.Counter()
    index = 2
    while index < count:
        opcode = tokens[index] & 0x7FF
        if opcode == 53:  # CUSTOMDATA stores its length in the following word.
            length = tokens[index + 1] if index + 1 < count else 0
            minimum = 2
        else:
            length = (tokens[index] >> 24) & 0x7F
            minimum = 1
        if not minimum <= length <= count - index:
            raise ValueError("invalid DXBC instruction length")
        if opcode in OPCODES:
            seen[OPCODES[opcode]] += 1
        index += length
    return dict(seen)


def read_blob(path):
    blob = path.read_bytes()
    stages = []
    offset = 0
    while (start := blob.find(b"DXBC", offset)) != -1:
        chunks, offset = _chunks(blob, start)
        if "RDEF" not in chunks:
            raise ValueError(f"DXBC stage in {path} is missing reflection")
        stage, buffers, bindings = _reflection(chunks["RDEF"])
        code = chunks.get("SHEX") or chunks.get("SHDR") or b""
        stat = chunks.get("STAT")
        stages.append({
            "stage": stage,
            "instructions": struct.unpack_from("<I", stat, 0)[0] if stat else None,
            "inputs": _signature(chunks.get("ISGN")),
            "outputs": _signature(chunks.get("OSGN")),
            "buffers": buffers,
            "bindings": bindings,
            "opcodes": _opcodes(code),
        })
    if not stages:
        raise ValueError(f"no DXBC stages in {path}")
    return {"blob": path.stem, "bytes": len(blob), "container": blob[:8].decode("latin1"), "stages": stages}


def describe(entry):
    declared, used, textures, comparisons = set(), set(), [], []
    for stage in entry["stages"]:
        for buffer in stage["buffers"]:
            for variable in buffer["variables"]:
                declared.add(variable["name"])
                if variable["used"]:
                    used.add(variable["name"])
        for binding in stage["bindings"]:
            if binding["type"] == "texture" and binding["name"] not in textures:
                textures.append(binding["name"])
            if binding["type"] == "sampler" and "Comparison" in binding["name"]:
                comparisons.append(binding["name"].replace("SamplerComparisonState", ""))
    if "g_TreeHeight" in declared:
        kind = "foliage"
    elif "g_OrientationUp" in declared:
        kind = "particle"
    elif "g_EffectModelMatrix" in declared:
        kind = "volumetric"
    elif "g_ViewportViewProjectionMatrices" in declared:
        kind = "shadowcaster"
    elif "g_Roughness" in declared and "g_TintColor" in declared:
        kind = "model"
    elif "g_Color4" in declared:
        kind = "image"
    else:
        kind = "effect/post"
    if kind == "foliage" and "g_ViewportViewProjectionMatrices" in declared:
        kind = "shadowcaster(foliage)"
    features = []
    if "g_Bones" in used:
        features.append("skinned")
    for prefix, label in (("g_FogDistance", "fogdist"), ("g_FogHeight", "fogheight")):
        if any(name.startswith(prefix) for name in declared):
            state = "USED" if any(name.startswith(prefix) for name in used) else "declared-unused"
            features.append(f"{label}:{state}")
    for name, label in (("g_LPoint_Origin", "point"), ("g_LSpot_Origin", "spot"), ("g_LDirectional_Direction", "directional"),
                        ("g_LFeature_ShadowProjection", "spotshadow"), ("g_LFeature_ShadowPointProjection", "pointshadow")):
        if name in used:
            features.append(label)
    if comparisons:
        features.append("shadow-sampler:" + ",".join(comparisons))
    if any(stage["stage"] == "GS" for stage in entry["stages"]):
        features.append("geometry-stage")
    pixel = next((stage for stage in entry["stages"] if stage["stage"] == "PS"), None)
    taps = sum(count for name, count in (pixel["opcodes"] if pixel else {}).items() if name.startswith("sample_c"))
    entry["summary"] = {
        "kind": kind,
        "features": features,
        "textures": textures,
        "shadow_compare_taps": taps,
        "pixel_discard": bool(pixel and pixel["opcodes"].get("discard")),
        "used_constants": sorted(used),
        "declared_unused_constants": sorted(declared - used),
    }
    return entry


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("item", type=pathlib.Path, help="workshop item directory, or a directory of .dxs blobs")
    parser.add_argument("--json", type=pathlib.Path, help="write the full manifest here")
    args = parser.parse_args(argv)
    directory = args.item / "shaders" / "blobsSM40"
    if not directory.is_dir():
        directory = args.item
    blobs = sorted(directory.glob("*.dxs"))
    if not blobs:
        parser.error(f"no .dxs blobs under {args.item}; run the wallpaper once in the native application first")
    entries = []
    for path in blobs:
        try:
            entries.append(describe(read_blob(path)))
        except (OSError, ValueError, struct.error) as error:
            parser.error(f"{path}: {error}")
    for entry in sorted(entries, key=lambda item: (item["summary"]["kind"], item["blob"])):
        summary = entry["summary"]
        pixel = next((stage for stage in entry["stages"] if stage["stage"] == "PS"), {})
        print(f"{summary['kind']:22} {entry['blob'][:8]} ps={pixel.get('instructions')!s:>4} "
              f"taps={summary['shadow_compare_taps']:>2} tex={','.join(name.replace('g_Texture', '') for name in summary['textures']) or '-':8} "
              f"{' '.join(summary['features'])}")
    counts = collections.Counter(entry["summary"]["kind"] for entry in entries)
    print(f"{len(entries)} programs: " + ", ".join(f"{count} {kind}" for kind, count in sorted(counts.items())))
    if args.json:
        args.json.write_text(json.dumps({"source": str(directory), "programs": entries}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
