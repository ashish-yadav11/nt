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
                    =*)
                        read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID")
                        rm -f "$pidfile"
                        ;;
                    *)
                        at -r "$id"
                        rm -f "$pidfile"
                        ;;
                esac
        done
        ;;
    *)
        id=$1
        pidfile=$(
            { at -c "$id" 2>/dev/null || { echo "ntrm: invalid id"; exit ;} ;} |
                awk -F'>' '
                    /^NT_MESSAGE=/ {e=1}
                    $1=="echo \"$$\" " {print $2; exit}
                    END {exit !e}
                '
        ) || { echo "ntrm: invalid id"; exit ;}
        if at -l | awk -v id="$id" '$1==id {exit $7!="="}' ; then
            read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID")
            rm -f "$pidfile"
        else
            at -r "$id"
            rm -f "$pidfile"
        fi
        ;;
esac
