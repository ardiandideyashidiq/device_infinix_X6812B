#!/usr/bin/env bash
# Host-side sepolicy validation without a build.
# Run from anywhere; locates $ANDROID_ROOT itself.
set -uo pipefail

TREE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT="$(cd "$TREE/../../.." && pwd)"
HOSTBIN="$ROOT/out/host/linux-x86/bin"
DEVICE_REL="device/infinix/X6812B"
SEPOLICY_REL="$DEVICE_REL/sepolicy/vendor"

[ -x "$HOSTBIN/checkpolicy" ] || { echo "FAIL: $HOSTBIN/checkpolicy missing (no build output)"; exit 1; }

# The m4 rule recorded in the ninja file lists every policy input as it existed at the
# last build. Files added since then are absent from that list, so replaying the rule
# verbatim validates the *previous* policy and reports OK while the new file is ignored.
# Rescan the tracked sepolicy dirs and splice in anything the build does not know about.
SYNC_SCRIPT="$ROOT/out/sepolicy-sync-inputs.py"
mkdir -p "$(dirname "$SYNC_SCRIPT")"
cat > "$SYNC_SCRIPT" <<'PY'
import json, os, shlex, sys

cmd_file, out_file = sys.argv[1], sys.argv[2]
root, device_rel, sepolicy_rel = sys.argv[3], sys.argv[4], sys.argv[5]

raw = open(cmd_file).read().strip()
parts = shlex.split(raw)
if '#' in parts:
    parts = parts[:parts.index('#')]
if '>' in parts:
    parts = parts[:parts.index('>')]

known = set(parts)
wanted = []
# sepolicy_rel already points at the vendor dir; the other two are siblings of it.
sepolicy_base = os.path.dirname(sepolicy_rel)
# Each config compiles a different set of directories. vendor_sepolicy.conf takes the
# device's vendor/ policy only; the neverallows config spans all of it. Including
# private/ or public/ rules in the vendor config makes checkpolicy reject types it
# never declares.
dirs = [sepolicy_rel]
if os.environ.get('SEPOLICY_MODULE', '').startswith('sepolicy_neverallows'):
    dirs += [os.path.join(sepolicy_base, 'private'), os.path.join(sepolicy_base, 'public')]
for d in dirs:
    if not os.path.isdir(d):
        continue
    for name in sorted(os.listdir(d)):
        # Only policy-language inputs belong on the m4 command line. file_contexts,
        # property_contexts, hwservice/service/vndservice contexts are compiled by soong
        # separately and are syntax errors to checkpolicy.
        if name.endswith('.te') or name == 'genfs_contexts':
            wanted.append(os.path.join(d, name))

# Only vendor-dir files belong in vendor_sepolicy.conf; contexts files for the other
# dirs are compiled elsewhere and would be rejected here.
wanted = [w for w in wanted if '/sepolicy/vendor/' in w or w.endswith('.te')]

missing = [w for w in wanted if w not in known]
added = []
if missing:
    # Anchor each new file next to a .te from the same directory so the insertion lands
    # inside the policy body (after global_macros/te_macros, before genfscon statements).
    for rel in missing:
        d = os.path.dirname(rel)
        peers = [i for i, a in enumerate(parts)
                 if os.path.dirname(a) == d and a.endswith('.te')]
        at = (max(peers) + 1) if peers else len(parts)
        parts.insert(at, rel)
        added.append(rel)

stale = [p for p in parts
         if p.startswith(sepolicy_rel + '/') and not os.path.exists(os.path.join(root, p))]

open(out_file, 'w').write(' '.join(shlex.quote(p) for p in parts) + '\n')
print(json.dumps({'added': added, 'stale': stale}))
PY

extract_rule() {
    local module="$1" out="$2"
    MODULE="$module" OUT="$out" ROOT="$ROOT" python3 - <<'PY'
import glob, os, sys
mod = '# Module:  ' + os.environ['MODULE']
root = os.environ['ROOT']
candidates = [p for p in sorted(glob.glob(os.path.join(root, 'out/soong/build.lineage_*.ninja')))
              if mod in open(p, errors='replace').read()]
if not candidates:
    sys.exit('FAIL: rule for %s not found in any split ninja; out/ is stale, ask for a build'
             % os.environ['MODULE'])
lines = open(candidates[0], errors='replace').read().splitlines()
start = next(i for i, l in enumerate(lines) if l.strip() == mod)
for i in range(start, start + 40):
    if lines[i].startswith('    command = '):
        cmd = lines[i].split('command = ', 1)[1]
        j = i + 1
        while j < len(lines) and lines[j].startswith('        ') and '=' not in lines[j][:20]:
            cmd += lines[j].strip()
            j += 1
        open(os.environ['OUT'], 'w').write(cmd.split(' # hash')[0] + '\n')
        break
else:
    sys.exit('FAIL: command line for %s not found in %s' % (os.environ['MODULE'], candidates[0]))
PY
}

cd "$ROOT" || exit 1
RC=0

for spec in \
    "vendor_sepolicy.conf|$ROOT/out/soong/.intermediates/system/sepolicy/vendor_sepolicy.conf/android_common/X6812B/vendor_sepolicy.conf|-C -M" \
    "sepolicy_neverallows.checkpolicy.conf|$ROOT/out/soong/.intermediates/system/sepolicy/sepolicy_neverallows.checkpolicy.conf/android_common/sepolicy_neverallows.checkpolicy.conf|-M"
do
    module="${spec%%|*}"
    rest="${spec#*|}"
    conf="${rest%%|*}"
    flags="${rest##*|}"

    echo "=== $module"
    if [ ! -f "$conf" ]; then
        echo "FAIL: $conf missing; out/ is stale, ask for a build"
        RC=1
        continue
    fi

    cmd="$ROOT/out/sepolicy-run-$module.sh"
    mkdir -p "$(dirname "$cmd")"
    if ! MODULE="$module" OUT="$cmd" extract_rule "$module" "$cmd"; then
        RC=1
        continue
    fi

    synced="$ROOT/out/sepolicy-run-$module.synced.sh"
    if ! SPLICE=$(SEPOLICY_MODULE="$module" python3 "$SYNC_SCRIPT" "$cmd" "$synced" "$ROOT" "$DEVICE_REL" "$SEPOLICY_REL"); then
        echo "FAIL: could not reconcile policy inputs for $module"
        RC=1
        continue
    fi
    ADDED=$(printf '%s' "$SPLICE" | python3 -c 'import json,sys; print(len(json.load(sys.stdin)["added"]))' 2>/dev/null || echo 0)
    STALE=$(printf '%s' "$SPLICE" | python3 -c 'import json,sys; print(len(json.load(sys.stdin)["stale"]))' 2>/dev/null || echo 0)
    if [ "${ADDED:-0}" -gt 0 ]; then
        echo "NOTE: $ADDED policy file(s) added since the last build were spliced in:"
        printf '%s' "$SPLICE" | python3 -c 'import json,sys; [print("      +", a) for a in json.load(sys.stdin)["added"]]'
        echo "      Rebuild to confirm soong accepts them."
    fi
    if [ "${STALE:-0}" -gt 0 ]; then
        echo "WARN: $STALE file(s) in the build's input list no longer exist:"
        printf '%s' "$SPLICE" | python3 -c 'import json,sys; [print("      -", a) for a in json.load(sys.stdin)["stale"]]'
    fi

    spath="$ROOT/out/sepolicy-conf-$module.conf"
    if ! bash "$synced" > "$spath"; then
        echo "FAIL: m4 failed for $module"
        RC=1
        continue
    fi

    out="$ROOT/out/sepolicy-$module.cil"
    rm -f "$out"
    "$HOSTBIN/checkpolicy" $flags -c 30 -o "$out" "$spath"
    if [ ! -s "$out" ]; then
        echo "FAIL: checkpolicy produced no output for $module (its exit code is unreliable)"
        RC=1
    else
        echo "OK: $out ($(wc -c < "$out") bytes)"
    fi
done

exit $RC
