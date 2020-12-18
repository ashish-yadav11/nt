#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit ;}

remove_running() {
    pidfile=$(echo "$info" | awk -F'>' '$1=="echo \"$$\" " {print $2; exit}')
    read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID")
    rm -f "$pidfile"
}

remove_pending() {
    at -r "$id"
    rm -f "$(echo "$info" | awk -F'>' '$1=="echo \"$$\" " {print $2; exit}')"
}

case $1 in
    -a)
        printf "Remove all pending notification alarms? [y/N]: "
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit

        at -l | LC_ALL=C sort -k7,7 | awk '!x[$1] {x[$1]=1; print $7$1}' |
            while read -r job ; do
                id=${job#?}
                info=$(at -c "$id")
                echo "$info" | grep -qm1 "^NT_MESSAGE=" || continue
                case $job in
                    =*)
                        remove_running
                        ;;
                    *)
                        remove_pending
                        ;;
                esac
        done
        ;;
    *)
        id=$1
        info=$(at -c "$id" 2>/dev/null) || { echo "ntrm: invalid id"; exit ;}
        echo "$info" | grep -qm1 "^NT_MESSAGE=" || { echo "ntrm: invalid id"; exit ;}
        if at -l | awk -v id="$id" '$1==id {exit $7!="="}' ; then
            remove_running
        else
            remove_pending
        fi
        ;;
esac
