#!/bin/sh
cid='\033[32m'
ctm='\033[33m'
cur='\033[36m'
cdf='\033[0m'

at -l | LC_ALL=C sort -k6,6 -k3,3M -k4,5 |
    awk 'BEGIN {id=-1}; id!=$1 {printf "%s,%s %s %2d %s %4d,%s\n",$1,$2,$3,$4,$5,$6,$8; id=$1}' |
        while IFS='' read -r job ; do
            if [ -z "$flag" ] ; then
                printf 'time:\t'; LC_ALL=C date +%c; printf '\n'
                flag=1
            fi
            id="${job%%,*}"
            info="$(
                at -c "$id" | awk -v ORS='' '
                    /^NT_MESSAGE=/ {print $0"|"; e=1}
                    $1=="t=$((" {print $2; exit}
                    END {exit !e}
                '
            )" || continue
            job="${job#*,}"
            ur="${job#*,}"
            t="${info##*|}"
            if [ -n "$t" ] ; then
                tm="$(LC_ALL=C date +%c -d "@$t")"
            else
                tm="${job%,*}"
            fi
            eval "${info%|*}"
            printf "$cid%s\t$ctm%s $cur%s$cdf\n%s\n" "$id" "$tm" "$ur" "message: $NT_MESSAGE"
        done
