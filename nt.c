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
        "		relative - M[.M] or [H[.H]h][M[.M]m][Ss]\n" \
        "		absolute - [HH]:[MM]\n" \
        "Examples:\n" \
        "	nt 15 '15 minutes up'\n" \
        "	nt 30s '30 seconds up'\n" \
        "	nt 4.5 '4.5 minutes up'\n" \
        "	nt .5h 'half hour up'\n" \
        "	nt 2h5s '2 hours 5 seconds up'\n" \
        "	nt 11:15 '11:15 up'\n" \
        "	nt 1: '01:00 up'\n" \
        "	nt :5 '00:05 up'\n" \
        "	nt -a noon tomorrow -m 'noon time'"

#define ATSHELLWARNING                  "warning: commands will be executed using /bin/sh\n"
#define ATSYNTAXERROR                   "syntax error. Last token seen: "

#define COLID                           "\033[32m"
#define COLTM                           "\033[33m"
#define COLDF                           "\033[0m"

#define AT(arg)                         (char *[]){ "at", "-M", arg, NULL }
#define ISDIGIT(X)                      (X >= '0' && X <= '9')

enum { None, Second, Minute, Hour }; /* last special token in time specification */

int
getntmessage()
{
        char *ntm = NULL;
        size_t len = 0;
        ssize_t n;

        if (isatty(STDIN_FILENO))
                fputs(NTMESSAGEPROMPT, stdout);
        n = getline(&ntm, &len, stdin) - 1;
        if (n < 0 || ntm[0] == '\n' || ntm[0] == '\0') {
                free(ntm);
                return 0;
        }
        if (ntm[n] == '\n')
                ntm[n] = '\0';
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

/* the following function assumes that t points to a buffer of length 5 */
int
parsetime(char *arg, char *colon, char *t)
{
        switch (colon - arg) {
                case 0:
                        t[0] = t[1] = '0';
                        break;
                case 1:
                        if (!ISDIGIT(arg[0]))
                                return 0;
                        t[0] = '0', t[1] = arg[0];
                        break;
                case 2:
                        if (!ISDIGIT(arg[0]) || !ISDIGIT(arg[1]))
                                return 0;
                        t[0] = arg[0], t[1] = arg[1];
                        break;
                default:
                        return 0;
        }
        if (ISDIGIT(colon[1])) {
                if (ISDIGIT(colon[2]))
                        t[2] = colon[1], t[3] = colon[2];
                else if (colon[2] == '\0')
                        t[2] = '0', t[3] = colon[1];
                else
                        return 0;
        } else if (colon[1] == '\0')
                t[2] = t[3] = '0';
        else
                return 0;
        t[4] = '\0';
        return 1;
}

int
parseduration(char *arg, unsigned int *t)
{
        int last = None, period = 0;
        unsigned int i = 0, f = 0, d = 1, c = 0;

        for (; *arg != '\0'; arg++) {
                if (ISDIGIT(*arg)) {
                        if (period) {
                                f = 10 * f + *arg - '0';
                                d *= 10;
                        } else
                                i = 10 * i + *arg - '0';
                        continue;
                }
                switch (*arg) {
                        case '.':
                                if (period)
                                        return 0;
                                period = 1;
                                break;
                        case 'h':
                                if (last != None)
                                        return 0;
                                last = Hour;
                                c = 60 * 60 * i + (60 * 60 * f) / d;
                                i = 0, period = 0, f = 0;
                                break;
                        case 'm':
                                if (last != Hour && last != None)
                                        return 0;
                                last = Minute;
                                c += 60 * i + (60 * f) / d;
                                i = 0, period = 0, f = 0;
                                break;
                        case 's':
                                if (period || last == Second)
                                        return 0;
                                last = Second;
                                c += i;
                                i = 0;
                                break;
                        default:
                                return 0;
                }
        }
        switch (last) {
                case None:
                        c = 60 * i + (60 * f) / d;
                        break;
                default:
                        if (i || f)
                                return 0;
                        break;
        }
        *t = c;
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

        if (pipe(fdr) == -1 || pipe(fdw) == -1) {
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
                        if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
                                close(fdw[1]);
                                close(fdr[0]);
                                perror("callat - signal");
                                exit(1);
                        }
                        if (t) {
                                int fd;
                                char tmp[] = "/var/tmp/nt-XXXXXX";

                                if ((fd = mkstemp(tmp)) == -1) {
                                        perror("callat - mkstemp");
                                        exit(1);
                                }
                                close(fd);
                                dprintf(fdw[1], "echo \"$$\" >%1$s\n"
                                                "while true ; do\n"
                                                "    t=$(( %2$jd - $(date +%%s) ))\n"
                                                "    [ \"$t\" -le 0 ] && break\n"
                                                "    sleep \"$t\"\n"
                                                "done\n"
                                                "%3$s \"$NT_MESSAGE\"\n"
                                                "rm -f %1$s",
                                                        tmp, (intmax_t)t, NOTIFY);
                        } else
                                dprintf(fdw[1], "%s \"$NT_MESSAGE\"", NOTIFY);
                        close(fdw[1]);
                        if (!(stream = fdopen(fdr[0], "r"))) {
                                close(fdr[0]);
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
                char arg[5];

                if (parsetime(argv[1], c, arg)) {
                        callat(0, AT(arg));
                        return 0;
                }
        } else if (parseduration(argv[1], &t)) {
                if (t < 120)
                        callat(time(NULL) + t, AT("now"));
                else {
                        char arg[23];

                        snprintf(arg, sizeof arg, "now + %u minutes", t / 60 - 1);
                        callat(time(NULL) + t, AT(arg));
                }
                return 0;
        }
        fputs("nt: invalid time specification\n", stderr);
        return 2;
}
