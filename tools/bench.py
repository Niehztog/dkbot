#!/usr/bin/env python3
"""Measure bot behaviour: A/B arms over paired seeds, in parallel, fast.

An ARM is `name[=tree][,VAR=value,...]`: the checkout whose build/ and
botdata/ to run (default: this one) and environment for its runs, e.g.

    tools/bench.py new old=/tmp/dkbot-old 'off=,DK_BOTLIB=none'
"""
import argparse, math, os, re, subprocess, threading, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = "/tmp/dkbench"
MAP_DEATHS = ("was frozen solid.", "was slimed.", "goes crispy.",
              "sucked it down.", "succumbed to the poison.")
SELF_KILLS = ("ate a full load of C4.", "failed at life.")


def run(tree, label, mapname, frames, port, seed, env):
    log = os.path.join(OUT, label + ".log")
    if os.path.exists(log):
        os.remove(log)
    e = dict(os.environ, DK_PORT=str(port), DK_LOG=log, DK_SEED=str(seed),
             DK_RUNDIR=os.path.join(OUT, "rd%d" % port), DK_BOT_SPAWN="4",
             DK_SHIM=os.path.join(ROOT, "build", "dk_preload.so"),
             DK_ARGS="+set fixedtime 100 +set gib_enable 0 "
             + os.environ.get("DK_ARGS", ""))
    e.update(env)
    p = subprocess.Popen([os.path.join(tree, "tools", "run-dkded.sh"), mapname,
                          "3600"], env=e, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, start_new_session=True)
    want, last, size, why = "frame %d:" % frames, time.time(), -1, "stalled"
    while p.poll() is None:
        time.sleep(2)
        text = open(log, errors="replace").read() if os.path.exists(log) else ""
        if want in text:
            why = None
            break
        if "\nERROR: " in text:
            why = text.split("\nERROR: ", 1)[1].split("\n", 1)[0]
            break
        if len(text) != size:
            size, last = len(text), time.time()
        elif time.time() - last > 90:
            break
    os.killpg(p.pid, 15) if p.poll() is None else None
    # timeout(1) has its own process group, and a stalled server never gets SIGPIPE
    for pid in subprocess.run(["pgrep", "-f", "[t]imeout 3600 .*port %d " % port],
                              capture_output=True, text=True).stdout.split():
        try:
            os.killpg(int(pid), 15)
        except OSError:
            pass
    p.wait()
    return why


def score(label, frames):
    s = dict(frags=0, kills=0, map=0, self=0, c4=0, idle=0)
    for line in open(os.path.join(OUT, label + ".log"), errors="replace"):
        if line.startswith("[dkbot] frame %d:" % frames):
            s["frags"] = sum(int(f) for f in re.findall(r"\) (-?\d+)/\d+ ", line))
            break
        if line.startswith("[dkbot] frame ") and not line.startswith("[dkbot] frame 100:"):
            s["idle"] += sum(1 for hp, mv in re.findall(r" hp=(-?\d+) mv=(\d+) ", line)
                             if int(hp) > 0 and int(mv) < 32)
        if not line.startswith("[Bot]"):
            continue
        line = line.rstrip("\n")
        if line.count("[Bot]") >= 2:
            s["kills"] += 1
            s["c4"] += "made a mess of" in line
        elif line.endswith(MAP_DEATHS):
            s["map"] += 1
        elif line.endswith(SELF_KILLS):
            s["self"] += 1
            s["c4"] += line.endswith("C4.")
    return s


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("arms", nargs="+")
    ap.add_argument("-n", type=int, default=10, help="seeds per arm and map")
    ap.add_argument("--seed0", type=int, default=1, help="the first seed")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--maps", default="e4dm2,e2dm2,e1dm2a")
    ap.add_argument("--frames", type=int, default=3000, help="a multiple of 100")
    ap.add_argument("--port", type=int, default=27800, help="first port")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    arms = []
    for spec in a.arms:
        head, *kv = spec.split(",")
        name, _, tree = head.partition("=")
        arms.append((name, os.path.abspath(tree) if tree else ROOT,
                     dict(x.split("=", 1) for x in kv if x)))
    seeds = range(a.seed0, a.seed0 + a.n)
    jobs = [(name, tree, env, m, seed) for seed in seeds
            for m in a.maps.split(",") for name, tree, env in arms]
    failed, lock = {}, threading.Lock()

    def worker(slot):
        while True:
            with lock:
                if not jobs:
                    return
                name, tree, env, m, seed = jobs.pop(0)
            label = "%s_%s_%d" % (name, m, seed)
            why = run(tree, label, m, a.frames, a.port + 20 * slot, seed, env)
            if why:
                with lock:
                    failed[label] = why
    threads = [threading.Thread(target=worker, args=(k,)) for k in range(a.j)]
    [t.start() for t in threads]
    [t.join() for t in threads]

    for m in a.maps.split(","):
        for name, _tree, _env in arms:
            rows = [score("%s_%s_%d" % (name, m, s), a.frames)
                    for s in seeds
                    if "%s_%s_%d" % (name, m, s) not in failed]
            if not rows:
                print("%-10s %-8s no complete runs" % (name, m))
                continue
            cells = []
            for k in ("frags", "kills", "map", "self", "c4", "idle"):
                v = [r[k] for r in rows]
                mu = sum(v) / len(v)
                sd = math.sqrt(sum((x - mu) ** 2 for x in v) / (len(v) - 1)) \
                    if len(v) > 1 else 0.0
                cells.append("%s %5.1f (%.1f)" % (k, mu, sd))
            print("%-10s %-8s n=%-3d %s" % (name, m, len(rows), "  ".join(cells)))
    for label, why in sorted(failed.items()):
        print("  %s: %s" % (label, why))


if __name__ == "__main__":
    main()
