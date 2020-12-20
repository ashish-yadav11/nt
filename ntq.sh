#!/bin/sh
cid="\033[32m"
ctm="\033[33m"
cur="\033[36m"
cdf="\033[0m"

at -l | LC_ALL=C sort -k6,6 -k3,3M -k4,4 -k5,5 |
    awk '!x[$1] {x[$1]=1; printf "%s,%s %s %2d %s %4d,%s\n",$1,$2,$3,$4,$5,$6,$8}' |
        while read -r job ; do
            id=${job%%,*}
            info=$(at -c "$id")
            nt=$(echo "$info" | grep -m1 "^NT_MESSAGE=") || continue
            job=${job#*,}
            ur=${job#*,}
            if t=$(echo "$info" | awk '$1=="sleep" {print $3; e=1; exit}; END {exit !e}') ; then
                tm=$(LC_ALL=C date +%c -d "@$t")
            else
                tm=${job%,*}
            fi
            eval "$nt"
            printf "$cid%s\t$ctm%s $cur%s$cdf\n%s\n" "$id" "$tm" "$ur" "message: $NT_MESSAGE"
        done
