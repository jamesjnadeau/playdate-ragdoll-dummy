#!/usr/bin/env python3
"""Generates perf/scenarios/fight/input.json deterministically.

Hold A to walk; every 25 frames alternate left and right holds, each with a 3-frame crank burst
of 25 degrees per frame, so the crank-driven weapon swings while that side is held.
Usage: tools/gen_fight_input.py [output]   (default perf/scenarios/fight/input.json)
"""
import json
import os
import sys

FRAMES = 600
PERIOD = 25
BURST = 3
CRANK = 25.0


def build():
    ev = [{"frame": 5, "press": ["a"]}]
    held = None
    for f in range(PERIOD, FRAMES, PERIOD):
        side = "left" if (f // PERIOD) % 2 == 1 else "right"
        if held:
            ev.append({"frame": f, "release": [held]})
        ev.append({"frame": f, "press": [side]})
        held = side
        for k in range(BURST):
            ev.append({"frame": f + k, "crank": CRANK})
    return {"events": ev}


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "perf/scenarios/fight/input.json")
    with open(out, "w") as fh:
        json.dump(build(), fh, indent=1)


if __name__ == "__main__":
    main()
