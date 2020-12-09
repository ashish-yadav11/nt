#!/bin/sh
[ "$#" -eq 0 ] && { echo "Usage: ntrm -a|<id>"; exit ;}
case $1 in
    -a)
        printf "Remove all notification alarms (y/N):"
        read -r confirm
        [ "$confirm" != y ] && [ "$confirm" != Y ] && exit
        atq | cut -f1 | while read -r id ; do
            at -c "$id" | grep -q "^NT_MESSAGE=" && atrm "$id"
        done
        ;;
    *)
        atrm "$@"
        ;;
esac
