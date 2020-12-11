#!/bin/sh
c_id=$(tput setaf 2)
c_tm=$(tput setaf 3)
c_ur=$(tput setaf 6)
c_df=$(tput sgr0)

atq | LC_ALL=C sort -k6,6 -k3,3M -k4,4 -k5,5 |
    awk '!x[$1] {x[$1]=1; printf "%s,%s %s %02d %s %04d,%s\n",$1,$2,$3,$4,$5,$6,$8}' |
        while read -r job ; do
            id=${job%%,*}
            info=$(at -c "$id")
            nt=$(echo "$info" | grep "^NT_MESSAGE=") || continue
            job=${job#*,}
            ur=${job#*,}
            if t=$(echo "$info" | awk '$1=="sleep" {print $3; e=1; exit}; END {exit !e}') ; then
                tm=$(LC_TIME=C date +%c -d "@$t")
            else
                tm=${job%,*}
            fi
            eval "$nt"
            printf "%s\t%s\n%s\n" \
                "${c_id}${id}" "${c_tm}${tm} ${c_ur}${ur}${c_df}" \
                "message: $NT_MESSAGE"
        done
