#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "config.h"

#define USAGE \
        "Usage:\n" \
        "	nt -h|--help\n" \
        "	nt <time-specification> [<notification-message>...]\n" \
        "	nt -a <at-supported-time-specification>... [-m <notification-message>...]\n" \
        "\n" \
        "	time-specification:\n" \
        "		relative - [M[.M]][,S] or [H[.H]h][M[.M]m][Ss]\n" \
        "		absolute - [HH]:[MM][:[SS]]\n" \
        "Examples:\n" \
        "	nt 15 '15 minutes up'\n" \
        "	nt 8,30 '8 minutes 30 seconds up'\n" \
        "	nt 30s '30 seconds up'\n" \
        "	nt 4.5 '4.5 minutes up'\n" \
        "	nt .5h 'half an hour up'\n" \
        "	nt 2h5s '2 hours 5 seconds up'\n" \
        "	nt 11:15 '11:15 up'\n" \
        "	nt 1: '01:00 up'\n" \
        "	nt :5 '00:05 up'\n" \
        "	nt -a noon tomorrow -m 'noon time'"

#define SPOOLDIR                        "/var/spool/nt"

#define ATSHELLWARNING                  "warning: commands will be executed using /bin/sh\n"
#define ATSYNTAXERROR                   "syntax error. Last token seen: "

#define COLID                           "\033[32m"
#define COLTM                           "\033[33m"
#define COLDF                           "\033[0m"

#define AT(arg)                         (char *[]){ "at", "-M", arg, NULL }
#define ISDIGIT(X)                      (X >= '0' && X <= '9')
#define NUM(X)                          (X - '0')

enum { None, Cmma, Scnd, Mnte, Hour }; /* last special token in time specification */

int
getntmessage(void)
{
        char *ntm = NULL;
        size_t len = 0;
        ssize_t n;

        if (isatty(STDIN_FILENO)) {
                fputs(NTMESSAGEPROMPT, stdout);
                fflush(stdout);
                if ((n = getline(&ntm, &len, stdin) - 1) < 0) {
                        puts("");
                        free(ntm);
                        return 0;
                }
        } else {
                if ((n = getline(&ntm, &len, stdin) - 1) < 0) {
                        free(ntm);
                        return 0;
                }
        }
        if (ntm[n] == '\n')
                ntm[n] = '\0';
        if (ntm[0] == '\0') {
                free(ntm);
                return 0;
        }
        setenv("NT_MESSAGE", ntm, 1);
        free(ntm);
        return 1;
}

/* the following function assumes size >= 1 */
int
catntmessage(int size, char *array[])
{
        int i;
        char *c;
        char *ntm;
        size_t len[size];
        size_t sumlen = 0;

        for (int i = 0; i < size; i++)
                sumlen += len[i] = strlen(array[i]);
        if (!sumlen)
                return 0;
        ntm = malloc(sumlen + size);
        memcpy(ntm, array[0], len[0]);
        for (i = 1, c = ntm + len[0]; i < size; c += len[i], i++) {
                *(c++) = ' ';
                memcpy(c, array[i], len[i]);
        }
        *c = '\0';
        setenv("NT_MESSAGE", ntm, 1);
        free(ntm);
        return 1;
}

int
parsetimeduration(char *arg, char *cln1, time_t ct, unsigned int *t)
{
        char *cln2;
        time_t at;
        struct tm *atm = localtime(&ct);

        switch (cln1 - arg) {
                case 0:
                        atm->tm_hour = 0;
                        break;
                case 1:
                        if (!ISDIGIT(arg[0]))
                                return 0;
                        atm->tm_hour = NUM(arg[0]);
                        break;
                case 2:
                        if (!ISDIGIT(arg[0]) || !ISDIGIT(arg[1]))
                                return 0;
                        atm->tm_hour = 10 * NUM(arg[0]) + NUM(arg[1]);
                        break;
                default:
                        return 0;
        }
        if ((cln2 = strchr(cln1 + 1, ':'))) {
                switch (strlen(cln2 + 1)) {
                        case 0:
                                atm->tm_sec = 0;
                                break;
                        case 1:
                                if (!ISDIGIT(cln2[1]))
                                        return 0;
                                atm->tm_sec = NUM(cln2[1]);
                                break;
                        case 2:
                                if (!ISDIGIT(cln2[1]) || !ISDIGIT(cln2[2]))
                                        return 0;
                                atm->tm_sec = 10 * NUM(cln2[1]) + NUM(cln2[2]);
                                break;
                        default:
                                return 0;
                }
                *cln2 = '\0';
        } else {
                atm->tm_sec = 0;
        }
        switch (strlen(cln1 + 1)) {
                case 0:
                        atm->tm_min = 0;
                        break;
                case 1:
                        if (!ISDIGIT(cln1[1]))
                                return 0;
                        atm->tm_min = NUM(cln1[1]);
                        break;
                case 2:
                        if (!ISDIGIT(cln1[1]) || !ISDIGIT(cln1[2]))
                                return 0;
                        atm->tm_min = 10 * NUM(cln1[1]) + NUM(cln1[2]);
                        break;
                default:
                        return 0;
        }

        at = mktime(atm);
        if (at < ct)
                at += 24 * 60 * 60;
        *t = at - ct;
        return 1;
}

int
parseduration(char *arg, unsigned int *t)
{
        int last = None, period = 0;
        unsigned int nbp = 0, nap = 0, scale = 1;
        unsigned int s = 0;

        for (; *arg != '\0'; arg++) {
                if (ISDIGIT(*arg)) {
                        if (period) {
                                nap = 10 * nap + NUM(*arg);
                                scale *= 10;
                        } else
                                nbp = 10 * nbp + NUM(*arg);
                        continue;
                }
                switch (*arg) {
                        case '.':
                                if (period)
                                        return 0;
                                period = 1;
                                break;
                        case ',':
                                if (last != None)
                                        return 0;
                                period = 0, last = Cmma;
                                s += 60 * nbp + (60 * nap) / scale;
                                nbp = 0, nap = 0;
                                break;
                        case 'h':
                                if (last == Cmma || last != None)
                                        return 0;
                                period = 0, last = Hour;
                                s += 60 * 60 * nbp + (60 * 60 * nap) / scale;
                                nbp = 0, nap = 0;
                                break;
                        case 'm':
                                if (last == Cmma || last == Mnte || last == Scnd)
                                        return 0;
                                period = 0, last = Mnte;
                                s += 60 * nbp + (60 * nap) / scale;
                                nbp = 0, nap = 0;
                                break;
                        case 's':
                                if (last == Cmma || last == Scnd)
                                        return 0;
                                if (period)
                                        return 0;
                                last = Scnd;
                                s += nbp;
                                nbp = 0;
                                break;
                        default:
                                return 0;
                }
        }
        switch (last) {
                case None:
                        s = 60 * nbp + (60 * nap) / scale;
                        break;
                case Cmma:
                        if (period)
                                return 0;
                        s += nbp;
                        break;
                default:
                        if (nbp || nap)
                                return 0;
                        break;
        }
        *t = s;
        return 1;
}

int
processatoutput(FILE *stream, time_t t)
{
        int s = 1, n = 0;
        char *line = NULL;
        size_t len = 0;
        intmax_t id;

        while (getline(&line, &len, stream) != -1) {
                if (s && strcmp(line, ATSHELLWARNING) == 0) {
                        s = 0;
                        continue;
                }
                if (!n) {
                        sscanf(line, "job %jd at %n", &id, &n);
                        if (n) {
                                if (t)
                                        printf("id: %s%jd%s, scheduled at: %s%s%s",
                                               COLID, id, COLDF, COLTM, ctime(&t), COLDF);
                                else
                                        printf("id: %s%jd%s, scheduled at: %s%s%s",
                                               COLID, id, COLDF, COLTM, line + n, COLDF);
                                continue;
                        }
                }
                if (strncmp(line, ATSYNTAXERROR, sizeof ATSYNTAXERROR - 1) == 0) {
                        fputs("nt: invalid time specification\n", stderr);
                        free(line);
                        return 0;
                }
                fputs(line, stdout);
        }
        free(line);
        return 1;
}

void
callat(time_t t, char *at[])
{
        int fdr[2], fdw[2];

        if (pipe(fdw) == -1) {
                perror("callat - pipe");
                exit(1);
        }
        if (pipe(fdr) == -1) {
                perror("callat - pipe");
                exit(1);
        }
        switch (fork()) {
                case -1:
                        perror("callat - fork");
                        exit(1);
                case 0:
                        close(fdw[1]);
                        close(fdr[0]);
                        if (fdw[0] != STDIN_FILENO) {
                                if (dup2(fdw[0], STDIN_FILENO) != STDIN_FILENO) {
                                        perror("callat - child - dup2");
                                        exit(1);
                                }
                                close(fdw[0]);
                        }
                        if (fdr[1] != STDOUT_FILENO) {
                                if (dup2(fdr[1], STDOUT_FILENO) != STDOUT_FILENO) {
                                        perror("callat - child - dup2");
                                        exit(1);
                                }
                                close(fdr[1]);
                        }
                        if (dup2(STDOUT_FILENO, STDERR_FILENO) != STDERR_FILENO) {
                                perror("callat - child - dup2");
                                exit(1);
                        }
                        execvp(at[0], at);
                        perror("callat - child - execvp");
                        _exit(127);
                default:
                {
                        FILE *stream;

                        close(fdw[0]);
                        close(fdr[1]);
                        /* ignore sigpipe for now (in case child exits early) */
                        signal(SIGPIPE, SIG_IGN);
                        if (t) {
                                int fd;
                                char pidfile[] = SPOOLDIR"/XXXXXX";

                                if ((fd = mkstemp(pidfile)) == -1) {
                                        perror("callat - mkstemp");
                                        exit(1);
                                }
                                close(fd);
                                dprintf(fdw[1], "echo \"$$\" >%1$s\n"
                                                "while true ; do\n"
                                                "    t=$(( %2$jd - $(date +%%s) ))\n"
                                                "    [ \"$t\" -le 0 ] && break\n"
                                                "    sleep \"$t\" && break\n"
                                                "done\n"
                                                "%3$s \"$NT_MESSAGE\"\n"
                                                "rm -f %1$s",
                                                        pidfile, (intmax_t)t, NOTIFY);
                        } else
                                dprintf(fdw[1], "%s \"$NT_MESSAGE\"", NOTIFY);
                        close(fdw[1]);
                        /* restore default sigpipe handler */
                        signal(SIGPIPE, SIG_DFL);

                        if (!(stream = fdopen(fdr[0], "r"))) {
                                perror("callat - fdopen");
                                exit(1);
                        }
                        if (!processatoutput(stream, t)) {
                                fclose(stream);
                                exit(2);
                        }
                        fclose(stream);
                }
        }
}

int
main(int argc, char *argv[])
{
        time_t ct;
        unsigned int t;
        char *c;

        if (argc < 2) {
                fputs("nt: not enough arguments\n", stderr);
                return 2;
        }
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
                puts(USAGE);
                return 0;
        }
        if (strcmp(argv[1], "-a") == 0) {
                int i;

                if (argc < 3) {
                        fputs("nt: not enough arguments\n", stderr);
                        return 2;
                }
                for (i = 3; i < argc && strcmp(argv[i], "-m") != 0; i++);
                if (i++ == argc ? !getntmessage() : !catntmessage(argc - i, argv + i)) {
                        fputs("nt: notification message can't be empty\n", stderr);
                        return 2;
                }
                {
                        char *at[i + 1];

                        at[0] = "at", at[1] = "-M", at[2] = "--", at[i] = NULL;
                        for (int j = 3; j < i; j++)
                                at[j] = argv[j - 1];
                        callat(0, at);
                }
                return 0;
        }
        if (argc == 2 ? !getntmessage() : !catntmessage(argc - 2, argv + 2)) {
                fputs("nt: notification message can't be empty\n", stderr);
                return 2;
        }
        if ((c = strchr(argv[1], ':'))) {
                ct = time(NULL);
                if (!parsetimeduration(argv[1], c, ct, &t))
                        goto error;
        } else {
                if (!parseduration(argv[1], &t))
                        goto error;
                ct = time(NULL);
        }
        if (t < 120)
                callat(ct + t, AT("now"));
        else {
                char arg[32];

                snprintf(arg, sizeof arg, "now + %u minutes", t / 60 - 1);
                callat(ct + t, AT(arg));
        }
        return 0;
error:
        fputs("nt: invalid time specification\n", stderr);
        return 2;
}
