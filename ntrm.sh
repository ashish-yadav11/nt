#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit ;}
case $1 in
    -a)
        printf "Remove all pending notification alarms (y/N):"
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit
        atq | LC_ALL=C sort -k7,7 | awk '!x[$1] {x[$1]=1; print $7$1}' |
            while read -r job ; do
                id=${job#?}
                info=$(at -c "$id")
                echo "$info" | grep -q "^NT_MESSAGE=" || exit
                case $job in
                    =*)
                        eval "$(echo "$info" | grep "^NT_PIDFILE=")"
                        read -r PID <"$NT_PIDFILE"
                        /usr/bin/kill -- "-$PID"
                        rm -f "$NT_PIDFILE"
                        ;;
                    *)
                        atrm "$id"
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
        atrm "$1"
        if pidline=$(echo "$info" | grep "^NT_PIDFILE=") ; then
            eval "$pidline"
            rm -f "$NT_PIDFILE"
        fi
        ;;
esac
