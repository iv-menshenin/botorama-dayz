#!/bin/bash
# Start DayZServer fully detached: setsid (new session, survives caller exit) +
# stdin redirected from /dev/null (so the caller's pipe is not held open, which
# otherwise makes shell wrappers like the agent's bash tool hang until timeout).
pkill -f DayZServer 2>/dev/null || true
sleep 2
setsid nohup /home/devalio/dayz-cherno < /dev/null > /tmp/dayz_console.log 2>&1 &
echo "server started pid=$!"
