#!/bin/sh
at -l | awk '$7=="=" {print $1}' |
    while read -r id ; do
        pidfile=$(
            at -c "$id" | awk -F'>' '
                /^NT_MESSAGE=/ {e=1}
                $1=="echo \"$$\" " {print $2; exit}
                END {exit !e}
            '
        ) && read -r PID 2>/dev/null <"$pidfile" && pkill -P "$PID" sleep
    done
