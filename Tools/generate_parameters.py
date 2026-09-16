#!/usr/bin/env python3
"""Generate the stable C++ parameter catalogue from JSFX slider declarations."""

from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "LJuno-116.jsfx"
EXTENSION_SOURCE = ROOT / "LJuno-116 Params.jsfx"
EXTENSION_SLIDER_OFFSET = 284
OUTPUT = ROOT / "Source" / "GeneratedParameters.h"
LUA_SOURCE = ROOT / "LJuno-116.lua"
PAGES_OUTPUT = ROOT / "Source" / "GeneratedPages.h"

SLIDER_RE = re.compile(
    r"^slider(?P<number>\d+):(?P<default>[-+0-9.eE]+)"
    r"<(?P<minimum>[-+0-9.eE]+),(?P<maximum>[-+0-9.eE]+),"
    r"(?P<step>[-+0-9.eE]+)(?:\{(?P<choices>[^}]*)\})?>"
    r"(?P<name>.+?)\s*$"
)

# Parameters already implemented directly in the VST3 after the original
# JSFX reached its slider ceiling. They remain part of the stable VST3 API
# even when an updated 256-slider JSFX reference does not repeat them.
VST_ONLY_DECLARATIONS = """
slider257:0<0,6,1{Off,SID Sequence,Ascending,Descending,Up and Down,Free Played Order,Random}>Layer 1 Arp 2 Pattern
slider258:16<0.125,128,0.125>Layer 1 Arp 2 Speed
slider259:0<0,4,1>Layer 1 Arp 2 Octaves Above
slider260:0<-4,0,1>Layer 1 Arp 2 Octaves Below
slider261:0<0,1,0.001>Layer 1 Arp 2 Glide
slider262:0<-4,4,0.01>Arp 2 Low Pass Movement
slider263:0<-4,4,0.01>Arp 2 High Pass Movement
slider264:0<-1,1,0.01>Arp 2 Stereo Movement
slider265:0<-1,1,0.01>Layer 1 Arp 2 Level Movement
slider266:0<-1,1,0.001>Layer 1 Arp 2 Pulse Width Movement
slider267:0<0,6,1{Off,SID Sequence,Ascending,Descending,Up and Down,Free Played Order,Random}>Layer 2 Arp 2 Pattern
slider268:16<0.125,128,0.125>Layer 2 Arp 2 Speed
slider269:0<0,4,1>Layer 2 Arp 2 Octaves Above
slider270:0<-4,0,1>Layer 2 Arp 2 Octaves Below
slider271:0<0,1,0.001>Layer 2 Arp 2 Glide
slider272:0<-1,1,0.01>Layer 2 Arp 2 Level Movement
slider273:0<-1,1,0.001>Layer 2 Arp 2 Pulse Width Movement
slider274:0<0,1,0.01>LFO1 Upper Squash
slider275:0<0,1,0.01>LFO1 Lower Squash
slider276:0<0,1,0.01>LFO2 Upper Squash
slider277:0<0,1,0.01>LFO2 Lower Squash
slider278:1<0,1,1{Off,On}>Layer 1 Arp 2 Pitch Movement
slider279:1<0,1,1{Off,On}>Layer 2 Arp 2 Pitch Movement
slider280:0<0,10,1{Classic Color,White,Pink,Brown,Radio Static,Radio Tuning,Radio Crackle,Biscot SID 6581,Amstrad CPC AY,8-Bit Metallic,Digital Dust}>Noise Type
slider281:0<-48,48,0.01>Noise Pitch
slider282:0<-48,48,0.01>LFO1 Noise Pitch
slider283:0<-48,48,0.01>LFO2 Noise Pitch
slider284:0<0,1,0.01>Noise Stereo
""".strip().splitlines()

LUA_PARAMETER_ALIASES = {
    "LArp BPM Rate Pattern Shape": "LArp BPM Division Pattern",
    "LArp BPM Rate Pattern Cycle": "LArp BPM Division Pattern Speed",
}

# The VST3 exposes one additional routing combination for the integrated
# LArp.  Keep it as the final choice so values 0, 1 and 2 retain exactly the
# same meaning in existing projects and presets.  The JSFX remains an input
# reference and is not modified for this VST3-only extension.
VST_PARAMETER_OVERRIDES = {
    # VST3-only effect engine selectors. Values 0 and 1 preserve the historical
    # Off/On meanings; value 2 selects the LWS-7 engine.
    100: {
        "maximum": "2",
        "choices": "Off,Delay 1,Delay 2",
    },
    140: {
        "maximum": "2",
        "choices": "Off,Reverb 1,Reverb 2",
    },
    202: {
        "maximum": "3",
        "choices": "Synth Direct,Synth + LArp,LArp MIDI Only,Synth LArp + MIDI Direct",
    },
}

LUA_PARAM_BINDING_RE = re.compile(
    r'select_param\(\s*"([^"]+)"\s*,\s*[^,\r\n]+,\s*[^,\r\n]+,'
    r'\s*[^,\r\n]+,\s*"([^"]+)"'
)
LUA_COMBO_BINDING_RE = re.compile(
    r'select_combo\(\s*"([^"]+)"\s*,\s*[^,\r\n]+,\s*[^,\r\n]+,'
    r'\s*"([^"]+)"'
)


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def cpp_float(value: str) -> str:
    rendered = format(float(value), ".9g")
    if "." not in rendered and "e" not in rendered.lower():
        rendered += ".0"
    return rendered + "f"


def lua_bindings(source: str) -> list[tuple[str, str]]:
    """Return (internal parameter name, user-facing label) in Lua table order."""
    matches = [
        (match.start(), match.group(1), match.group(2))
        for pattern in (LUA_PARAM_BINDING_RE, LUA_COMBO_BINDING_RE)
        for match in pattern.finditer(source)
    ]
    return [(internal, label) for _, internal, label in sorted(matches)]


def main() -> None:
    parameters: list[dict[str, str]] = []
    for line_number, line in enumerate(SOURCE.read_text(encoding="utf-8").splitlines(), 1):
        if re.match(r"^slider\d+:", line) is None:
            continue
        match = SLIDER_RE.match(line)
        if match is None:
            raise RuntimeError(f"Unsupported slider declaration at line {line_number}: {line}")
        parameters.append(match.groupdict(default=""))

    for line in VST_ONLY_DECLARATIONS:
        match = SLIDER_RE.match(line)
        if match is None:
            raise RuntimeError(f"Unsupported VST-only slider declaration: {line}")
        parameters.append(match.groupdict(default=""))

    # The companion JSFX only exists to bypass REAPER's 256-slider ceiling.
    # In VST3 its three declarations become ordinary stable parameters after
    # the existing catalogue; no gmem or second plug-in is reproduced.
    for line_number, line in enumerate(EXTENSION_SOURCE.read_text(encoding="utf-8").splitlines(), 1):
        if re.match(r"^slider\d+:", line) is None:
            continue
        match = SLIDER_RE.match(line)
        if match is None:
            raise RuntimeError(
                f"Unsupported extender slider declaration at line {line_number}: {line}"
            )
        parameter = match.groupdict(default="")
        parameter["number"] = str(EXTENSION_SLIDER_OFFSET + int(parameter["number"]))
        parameters.append(parameter)

    for parameter in parameters:
        override = VST_PARAMETER_OVERRIDES.get(int(parameter["number"]))
        if override is not None:
            parameter.update(override)

    numbers = [int(p["number"]) for p in parameters]
    if len(numbers) != len(set(numbers)):
        raise RuntimeError("Duplicate JSFX slider number")

    lua = LUA_SOURCE.read_text(encoding="utf-8")
    public_name_by_internal: dict[str, str] = {}
    for internal_name, public_name in lua_bindings(lua):
        internal_name = LUA_PARAMETER_ALIASES.get(internal_name, internal_name)
        public_name_by_internal.setdefault(internal_name, public_name)

    rows = []
    for parameter in sorted(parameters, key=lambda p: int(p["number"])):
        number = int(parameter["number"])
        choices = parameter["choices"].replace(",", "|")
        public_name = public_name_by_internal.get(parameter["name"], parameter["name"])
        rows.append(
            "    { %d, \"slider%03d\", \"%s\", \"%s\", %s, %s, %s, %s, \"%s\" },"
            % (
                number,
                number,
                cpp_string(parameter["name"]),
                cpp_string(public_name),
                cpp_float(parameter["minimum"]),
                cpp_float(parameter["maximum"]),
                cpp_float(parameter["step"]),
                cpp_float(parameter["default"]),
                cpp_string(choices),
            )
        )

    content = """// Generated by Tools/generate_parameters.py. Do not edit by hand.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace ljuno::generated
{
struct ParameterDescriptor
{
    int sliderNumber;
    const char* id;
    const char* name;
    const char* displayName;
    float minimum;
    float maximum;
    float step;
    float defaultValue;
    const char* choices;
};

inline constexpr ParameterDescriptor parameters[] = {
""" + "\n".join(rows) + "\n};\n}\n"

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(content, encoding="utf-8", newline="\n")
    print(f"Generated {len(parameters)} parameters in {OUTPUT}")

    generate_pages(parameters)


def generate_pages(parameters: list[dict[str, str]]) -> None:
    lua = LUA_SOURCE.read_text(encoding="utf-8")
    parameter_by_name = {parameter["name"]: int(parameter["number"]) for parameter in parameters}
    preserved_pages: dict[str, tuple[list[str], list[str]]] = {}
    if PAGES_OUTPUT.exists():
        old_pages = PAGES_OUTPUT.read_text(encoding="utf-8")
        old_id_arrays = {
            name: re.findall(r'"([^"]+)"', body)
            for name, body in re.findall(
                r"inline constexpr const char\* (\w+ParameterIds)\[\] = \{(.*?)\};",
                old_pages,
                re.DOTALL,
            )
        }
        old_name_arrays = {
            name: re.findall(r'"([^"]+)"', body)
            for name, body in re.findall(
                r"inline constexpr const char\* (\w+ParameterNames)\[\] = \{(.*?)\};",
                old_pages,
                re.DOTALL,
            )
        }
        for page_name, id_array, name_array in re.findall(
            r'\{\s*"([^"]+)",\s*(\w+ParameterIds),\s*(\w+ParameterNames),',
            old_pages,
        ):
            if id_array in old_id_arrays and name_array in old_name_arrays:
                preserved_pages[page_name] = (
                    old_id_arrays[id_array], old_name_arrays[name_array]
                )
    page_tables = [
        ("Osc", "osc_bindings"),
        ("Filter", "filter_bindings"),
        ("Filter Routing", "filter_routing_bindings"),
        ("Env", "env_bindings"),
        ("Micro Motion", "micro_motion_bindings"),
        ("LFO 1", "lfo1_bindings"),
        ("LFO 2", "lfo2_bindings"),
        ("EQ", "eq_bindings"),
        ("FX", "fx_bindings"),
        ("SuperWave L1", "superwave_l1_bindings"),
        ("SuperWave L2", "superwave_l2_bindings"),
        ("Arp", "arp_bindings"),
        ("Arp Modulation", "arp_mod_bindings"),
        ("Arp 2", "arp2_bindings"),
        ("Global", "global_bindings"),
    ]

    page_rows: list[str] = []
    id_arrays: list[str] = []
    name_arrays: list[str] = []
    for page_index, (page_name, table_name) in enumerate(page_tables):
        if page_name not in {"Filter", "Filter Routing"} and page_name in preserved_pages:
            preserved_ids, preserved_names = preserved_pages[page_name]
            array_name = f"page{page_index}ParameterIds"
            id_arrays.append(
                f"inline constexpr const char* {array_name}[] = {{ "
                + ", ".join(f'\"{cpp_string(value)}\"' for value in preserved_ids)
                + " };"
            )
            names_array_name = f"page{page_index}ParameterNames"
            name_arrays.append(
                f"inline constexpr const char* {names_array_name}[] = {{ "
                + ", ".join(f'\"{cpp_string(value)}\"' for value in preserved_names)
                + " };"
            )
            page_rows.append(
                f'    {{ "{cpp_string(page_name)}", {array_name}, {names_array_name}, std::size ({array_name}) }},'
            )
            continue

        table_match = re.search(
            rf"local\s+{re.escape(table_name)}\s*=\s*\{{(?P<body>.*?)\n\}}",
            lua,
            re.DOTALL,
        )
        if table_match is None:
            raise RuntimeError(f"Lua binding table not found: {table_name}")

        bindings = lua_bindings(table_match.group("body"))
        slider_ids: list[str] = []
        public_names: list[str] = []
        missing: list[str] = []
        for name, public_name in bindings:
            slider_number = parameter_by_name.get(LUA_PARAMETER_ALIASES.get(name, name))
            if slider_number is None:
                # Bypass and Wet are REAPER's host parameters, not JSFX sliders.
                missing.append(name)
                continue
            slider_ids.append(f'"slider{slider_number:03d}"')
            public_names.append(f'"{cpp_string(public_name)}"')

        array_name = f"page{page_index}ParameterIds"
        id_arrays.append(
            f"inline constexpr const char* {array_name}[] = {{ {', '.join(slider_ids)} }};"
        )
        names_array_name = f"page{page_index}ParameterNames"
        name_arrays.append(
            f"inline constexpr const char* {names_array_name}[] = {{ {', '.join(public_names)} }};"
        )
        page_rows.append(
            f'    {{ "{cpp_string(page_name)}", {array_name}, {names_array_name}, std::size ({array_name}) }},'
        )
        if missing:
            print(f"Skipped non-JSFX host parameters on {page_name}: {', '.join(missing)}")

    pages_content = """// Generated by Tools/generate_parameters.py from Lua PAGE_BINDINGS. Do not edit.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

namespace ljuno::generated
{
struct PageDescriptor
{
    const char* name;
    const char* const* parameterIds;
    const char* const* parameterNames;
    std::size_t parameterCount;
};

""" + "\n".join(id_arrays + name_arrays) + "\n\ninline constexpr PageDescriptor pages[] = {\n" + "\n".join(page_rows) + "\n};\n}\n"
    PAGES_OUTPUT.write_text(pages_content, encoding="utf-8", newline="\n")
    print(f"Generated {len(page_tables)} pages in {PAGES_OUTPUT}")


if __name__ == "__main__":
    main()
