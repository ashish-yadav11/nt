#!/bin/sh
c_id=$(tput setaf 2)
c_tm=$(tput setaf 3)
c_qu=$(tput setaf 4)
c_ur=$(tput setaf 6)
c_df=$(tput sgr0)

atq | sort -r -k6,6 -k3,3M -k4,4 -k5,5 |
    while read -r job ; do
        ur=${job##* }
        job=${job% *}
        qu=${job##* }
        # skip redundant description of running jobs
        if [ "$qu" = "=" ] ; then
            lastactive=1
            continue
        elif [ "$lastactive" = 1 ] ; then
            qu="~$qu"
        else
            qu=" $qu"
        fi
        job=${job% *}
        tm=${job#*	}
        id=${job%%	*}
        info=$(at -c "$id")
        nt=$(echo "$info" | grep "^NT_MESSAGE=") || continue
        eval "$nt"
        if t=$(echo "$info" | awk '$1=="sleep" {print $3; f=1; exit}; END {exit !f}') ; then
            tm=$(LC_TIME=C date +%c -d "@$t")
        fi
        printf "%s\t%s\n%s\n" \
            "${c_id}${id}" "${c_tm}${tm} ${c_qu}${qu} ${c_ur}${ur}${c_df}" \
            "message: $NT_MESSAGE"
    done
