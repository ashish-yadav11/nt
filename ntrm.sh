#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit 2 ;}

remove_by_id() {
    pidfile=$(
        { at -c "$1" 2>/dev/null || { echo "ntrm: invalid id" >&2; exit 1 ;} ;} |
            awk -F'>' '
                /^NT_MESSAGE=/ {e=1}
                $1=="echo \"$$\" " {print $2; exit}
                END {exit !e}
            '
    ) || { echo "ntrm: invalid id" >&2; exit 1 ;}

    if at -l | awk -v id="$1" '$1==id && $7=="=" {e=1; exit}; END {exit !e}' ; then
        read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID")
    else
        at -r "$1"
    fi
    rm -f "$pidfile"
}

remove_by_job() {
    id=${1#?}
    pidfile=$(
        at -c "$id" | awk -F'>' '
            /^NT_MESSAGE=/ {e=1}
            $1=="echo \"$$\" " {print $2; exit}
            END {exit !e}
        '
    ) || return 1

    case $1 in
        =*) read -r PID 2>/dev/null <"$pidfile" && kill "$PID" $(pgrep -P "$PID") ;;
         *) at -r "$id" ;;
    esac
    rm -f "$pidfile"
    return 0
}

case $1 in
    -a)
        printf "Remove all pending notification alarms? [y/N]: "
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit 0

        at -l | LC_ALL=C sort -k7,7 | awk 'BEGIN {id=-1}; id!=$1 {print $7$1}' |
            while IFS='' read -r job ; do
                remove_by_job "$job"
            done
        ;;
    -l)
        at -l | LC_ALL=C sort -r | awk 'BEGIN {id=-1}; id!=$1 {print $7$1}' |
            while IFS='' read -r job ; do
                remove_by_job "$job" && exit 0
            done
        ;;
    -n)
        at -l | LC_ALL=C sort -k6,6 -k3,3M -k4,4 -k5,5 | awk 'BEGIN {id=-1}; id!=$1 {print $7$1}' |
            while IFS='' read -r job ; do
                remove_by_job "$job" && exit 0
            done
        ;;
    *)
        remove_by_id "$1"
        ;;
esac
