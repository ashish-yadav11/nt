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
        "	nt -t <at-supported-time-specification> [<notification-message>...]\n" \
        "\n" \
        "	time-specification:\n" \
        "		relative - [H,]M[.S] or [Hh][Mm][Ss]\n" \
        "		absolute - [HH]:[MM]\n" \
        "Examples:\n" \
        "	nt 10 '10 minutes up'\n" \
        "	nt 30s '30 seconds up'\n" \
        "	nt 1, '1 hour up'\n" \
        "	nt 5.30 '5 minutes 30 seconds up'\n" \
        "	nt 2h5s '2 hours 5 seconds up'\n" \
        "	nt .10 '10 seconds up'\n" \
        "	nt 1,10 '1 hour 10 minutes up'\n" \
        "	nt 11:15 '11:15 up'\n" \
        "	nt 1: '01:00 up'\n" \
        "	nt :5 '00:05 up'\n" \
        "	nt -t 'noon tomorrow' 'noon time'"

#define UNWANTEDATWARNLINE              "warning: commands will be executed using /bin/sh\n"

#define COLID                           "\033[32m"
#define COLTM                           "\033[33m"
#define COLDF                           "\033[0m"

#define ISDIGIT(X)                      (X >= '0' && X <= '9')

/* last special token in time specification */
enum { None, Period, Comma, Second, Minute, Hour };

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

/* the following function assumes that atarg points to a buffer of length >= 5 */
int
parseatarg(char *arg, char *colon, char *atarg)
{
        switch (colon - arg) {
                case 0:
                        atarg[0] = atarg[1] = '0';
                        break;
                case 1:
                        if (!ISDIGIT(arg[0]))
                                return 0;
                        atarg[0] = '0', atarg[1] = arg[0];
                        break;
                case 2:
                        if (!ISDIGIT(arg[0]) || !ISDIGIT(arg[1]))
                                return 0;
                        atarg[0] = arg[0], atarg[1] = arg[1];
                        break;
                default:
                        return 0;
        }
        if (ISDIGIT(colon[1])) {
                if (ISDIGIT(colon[2]))
                        atarg[2] = colon[1], atarg[3] = colon[2];
                else if (colon[2] == '\0')
                        atarg[2] = '0', atarg[3] = colon[1];
                else
                        return 0;
        } else if (colon[1] == '\0')
                atarg[2] = atarg[3] = '0';
        else
                return 0;
        atarg[4] = '\0';
        return 1;
}

int
parsetime(char *arg, unsigned int *t)
{
        int last;
        unsigned int i;

        for (last = None, *t = 0, i = 0; *arg != '\0'; arg++) {
                if (ISDIGIT(*arg)) {
                        i = 10 * i + *arg - '0';
                        continue;
                }
                switch (*arg) {
                        case ',':
                                if (last != None)
                                        return 0;
                                last = Comma;
                                *t = 60 * 60 * i;
                                i = 0;
                                break;
                        case '.':
                                if (last != None && last != Comma)
                                        return 0;
                                last = Period;
                                *t += 60 * i;
                                i = 0;
                                break;
                        case 'h':
                                if (last != None)
                                        return 0;
                                last = Hour;
                                *t = 60 * 60 * i;
                                i = 0;
                                break;
                        case 'm':
                                if (last != None && last != Hour)
                                        return 0;
                                last = Minute;
                                *t += 60 * i;
                                i = 0;
                                break;
                        case 's':
                                if (last != None && last != Hour && last != Minute)
                                        return 0;
                                last = Second;
                                *t += i;
                                i = 0;
                                break;
                        default:
                                return 0;
                }
        }
        switch (last) {
                case None:
                        *t = 60 * i;
                        break;
                case Period:
                        *t += i;
                        break;
                case Comma:
                        *t += 60 * i;
                        break;
                default:
                        if (i)
                                return 0;
                        break;
        }
        return *t ? 1 : 0;
}

void
filteroutput(FILE *stream, time_t t)
{
        int u = 1, n = 0;
        char *line = NULL;
        size_t len = 0;
        intmax_t id;

        while (getline(&line, &len, stream) != -1) {
                if (u && strcmp(line, UNWANTEDATWARNLINE) == 0) {
                        u = 0;
                        continue;
                }
                if (!n) {
                        sscanf(line, "job %jd at %n", &id, &n);
                        if (n) {
                                if (t >= 0)
                                        printf("id: %s%jd%s, scheduled at: %s%s%s",
                                               COLID, id, COLDF, COLTM, ctime(&t), COLDF);
                                else
                                        printf("id: %s%jd%s, scheduled at: %s%s%s",
                                               COLID, id, COLDF, COLTM, line + n, COLDF);
                                continue;
                        }
                }
                fputs(line, stdout);
        }
        free(line);
}

void
callat(time_t t, char *atarg)
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
                {
                        char *arg[] = { "at", "-M", atarg, NULL };

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
                        execvp(arg[0], arg);
                        perror("callat - child - execvp");
                        _exit(127);
                }
                default:
                {
                        FILE *stream;

                        close(fdw[0]);
                        close(fdr[1]);
                        if (t >= 0) {
                                int fd;
                                char tmp[] = "/var/tmp/nt-XXXXXX";

                                if ((fd = mkstemp(tmp)) == -1) {
                                        perror("callat - mkstemp");
                                        exit(1);
                                }
                                close(fd);
                                dprintf(fdw[1], "echo \"$$\" >%1$s\n"
                                                "t=$(( %2$jd - $(date +%%s) ))\n"
                                                "[ \"$t\" -gt 0 ] && sleep \"$t\"\n"
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
                        filteroutput(stream, t);
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
                fputs("nt: not enough arguments\n" USAGE "\n", stderr);
                return 2;
        }
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
                puts(USAGE);
                return 0;
        }
        if (strcmp(argv[1], "-t") == 0) {
                if (argc < 3) {
                        fputs("nt: not enough arguments\n" USAGE "\n", stderr);
                        return 2;
                }
                if (argc == 3 ? !getntmessage() : !catntmessage(argc - 3, argv + 3)) {
                        fputs("nt: notification message can't be empty\n", stderr);
                        return 2;
                }
                callat(-1, argv[2]);
        } else {
                if (argc == 2 ? !getntmessage() : !catntmessage(argc - 2, argv + 2)) {
                        fputs("nt: notification message can't be empty\n", stderr);
                        return 2;
                }
                if ((c = strchr(argv[1], ':'))) {
                        char atarg[6];

                        if (parseatarg(argv[1], c, atarg)) {
                                callat(-1, atarg);
                                return 0;
                        }
                } else if (parsetime(argv[1], &t)) {
                        if (t < 120)
                                callat(time(NULL) + t, "now");
                        else {
                                char atarg[25];

                                snprintf(atarg, sizeof atarg, "now + %u minutes", t / 60 - 1);
                                callat(time(NULL) + t, atarg);
                        }
                        return 0;
                }
                fputs("nt: invalid time specification\n" USAGE "\n", stderr);
                return 2;
        }
        return 0;
}
