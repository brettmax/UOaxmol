#!/usr/bin/env bash
# End-to-end smoke test: synthetic UO data + fake shard + the real client under Xvfb.
# Usage: uo/tests/smoke/run_smoke.sh <client build dir> [screenshot.png]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:?client build dir}"
shot="${2:-$PWD/axmoluo-smoke.png}"
work="$(mktemp -d)"
port=2600

python3 "$here/make_test_data.py" "$work/uodata" >/dev/null
mkdir -p "$work/xdg/AxmolUO"
cat > "$work/xdg/AxmolUO/settings.json" <<JSON
{"uoDirectory":"$work/uodata","clientVersion":"7.0.15.1","host":"127.0.0.1","port":$port,
 "account":"smoke","password":"test","ignoreRelayAddress":true,"map":0,"autoLogin":true}
JSON

python3 "$here/fake_shard.py" "$port" > "$work/shard.log" 2>&1 &
shard=$!
sleep 1

cd "$build/bin/AxmolUO"
XDG_CONFIG_HOME="$work/xdg" AXMOLUO_SCREENSHOT="$shot" timeout 60 \
  xvfb-run -a -s "-screen 0 1280x720x24" ./AxmolUO > "$work/client.log" 2>&1
wait "$shard" || true

cat "$work/shard.log"
grep -q "playing: Lord Smoke" "$work/shard.log"
test -s "$shot"
echo "smoke test passed: $shot"
