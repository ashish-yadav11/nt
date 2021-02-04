#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit ;}
case $1 in
    -a)
        printf "Remove all pending notification alarms? [y/N]: "
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit

        at -l | LC_ALL=C sort -k7,7 | awk '!x[$1] {x[$1]=1; print $7$1}' |
            while read -r job ; do
                id=${job#?}
                pidfile=$(
                    at -c "$id" | awk -F'>' '
                        /^NT_MESSAGE=/ {e=1}
                        $1=="echo \"$$\" " {print $2; exit}
                        END {exit !e}
                    '
                ) || continue
                case $job in
                    =*) read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID") ;;
                     *) at -r "$id" ;;
                esac
                rm -f "$pidfile"
        done
        ;;
    *)
        pidfile=$(
            { at -c "$1" 2>/dev/null || { echo "ntrm: invalid id"; exit ;} ;} |
                awk -F'>' '
                    /^NT_MESSAGE=/ {e=1}
                    $1=="echo \"$$\" " {print $2; exit}
                    END {exit !e}
                '
        ) || { echo "ntrm: invalid id"; exit ;}
        if at -l | awk -v id="$1" '$1==id && $7=="=" {e=1; exit} END {exit !e}' ; then
            read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID")
        else
            at -r "$1"
        fi
        rm -f "$pidfile"
        ;;
esac
