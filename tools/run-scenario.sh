#!/bin/bash
# Фаза 4 — агентская обвязка E2E: «сценарий → poll результата → tail RPT».
#
# Кладёт сценарий в e2e/in/ (атомарно), ждёт out/<job>.result.json, печатает
# результат и при не-ok статусе tail'ит RPT сервера. Упрощает прогон одного
# сценария для тестировщика/агента (ручной цикл из docs/ai-testing-guide.md).
#
# Использование:
#   tools/run-scenario.sh <имя|путь.json> [--timeout <s>] [--poll <s>]
#   tools/run-scenario.sh movement-smoke --timeout 180
#
# Предполагает: сервер запущен, мост включён (файл e2e/enabled существует).
set -euo pipefail

E2E_DIR="/mnt/deep-space/Steam/steamapps/common/DayZServer/profiles-cherno/dmBotorama/e2e"
SCENARIOS="/home/devalio/dayz/Work/botorama/tools/e2e"
RPT_DIR="/mnt/deep-space/Steam/steamapps/common/DayZServer/profiles-cherno"

TIMEOUT=120
POLL=1

usage() { echo "usage: $(basename "$0") <name|path.json> [--timeout <s>] [--poll <s>]" >&2; exit 2; }

[ $# -ge 1 ] || usage
SCEN="$1"; shift

while [ $# -gt 0 ]; do
  case "$1" in
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --poll)    POLL="$2";    shift 2 ;;
    *) usage ;;
  esac
done

# Разрешить сценарий: по имени (в tools/e2e/) или по пути.
if [ -f "$SCEN" ]; then
  SCEN_PATH="$SCEN"
elif [ -f "$SCENARIOS/$SCEN.json" ]; then
  SCEN_PATH="$SCENARIOS/$SCEN.json"
else
  echo "scenario not found: $SCEN" >&2
  exit 2
fi

JOB_NAME="$(basename "$SCEN_PATH" .json)"
RESULT="$E2E_DIR/out/$JOB_NAME.result.json"

tail_rpt() {
  local last
  last="$(ls -t "$RPT_DIR"/*.RPT 2>/dev/null | head -1)"
  if [ -n "$last" ]; then
    echo "=== RPT tail: $(basename "$last") ==="
    tail -40 "$last"
  else
    echo "(no RPT found in $RPT_DIR)"
  fi
}

[ -f "$E2E_DIR/enabled" ] || { echo "e2e bridge not enabled ($E2E_DIR/enabled missing)" >&2; exit 3; }

# Убрать stale-результат от прошлого прогона с тем же именем.
rm -f "$RESULT"

# Атомарный деплой сценария: .tmp → rename.
mkdir -p "$E2E_DIR/in"
cp "$SCEN_PATH" "$E2E_DIR/in/$JOB_NAME.json.tmp"
mv "$E2E_DIR/in/$JOB_NAME.json.tmp" "$E2E_DIR/in/$JOB_NAME.json"

echo "=== dropped $JOB_NAME.json, polling $RESULT (timeout ${TIMEOUT}s) ==="

ELAPSED=0
while [ ! -f "$RESULT" ]; do
  if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
    echo "TIMEOUT: no result after ${TIMEOUT}s" >&2
    tail_rpt
    exit 4
  fi
  sleep "$POLL"
  ELAPSED=$((ELAPSED + POLL))
done

echo "=== result: $JOB_NAME ==="
cat "$RESULT"
echo

STATUS="$(grep -o '"Status"[[:space:]]*:[[:space:]]*"[^"]*"' "$RESULT" | head -1 | sed 's/.*"\([^"]*\)"$/\1/')"
if [ "$STATUS" != "ok" ]; then
  tail_rpt
  exit 1
fi

echo "PASS: $JOB_NAME (status=$STATUS)"
