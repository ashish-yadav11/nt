#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit ;}
case $1 in
    -a)
        printf "Remove all pending notification alarms (y/N): "
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit
        at -l | LC_ALL=C sort -k7,7 | awk '!x[$1] {x[$1]=1; print $7$1}' |
            while read -r job ; do
                id=${job#?}
                info=$(at -c "$id")
                echo "$info" | grep -q "^NT_MESSAGE=" || continue
                case $job in
                    =*)
                        eval "$(echo "$info" | grep "^NT_PIDFILE=")"
                        read -r PID <"$NT_PIDFILE" && kill "$PID" $(pgrep -P "$PID")
                        rm -f "$NT_PIDFILE"
                        ;;
                    *)
                        at -r "$id"
                        if pidline=$(echo "$info" | grep "^NT_PIDFILE=") ; then
                            eval "$pidline"
                            rm -f "$NT_PIDFILE"
                        fi
                        ;;
                esac
        done
        ;;
    *)
        info=$(at -c "$1" 2>/dev/null) || { echo "ntrm: invalid id"; exit ;}
        echo "$info" | grep -q "^NT_MESSAGE=" || { echo "ntrm: invalid id"; exit ;}
        if at -l | grep -q "^$1.* = " ; then
            if pidline=$(echo "$info" | grep "^NT_PIDFILE=") ; then
                eval "$pidline"
                read -r PID <"$NT_PIDFILE" && kill "$PID" $(pgrep -P "$PID")
                rm -f "$NT_PIDFILE"
            fi
        else
            at -r "$1"
            if pidline=$(echo "$info" | grep "^NT_PIDFILE=") ; then
                eval "$pidline"
                rm -f "$NT_PIDFILE"
            fi
        fi
        ;;
esac
