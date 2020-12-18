/* compilation:

gcc -o nt -O3 -Wall -Wextra nt.c

*/

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
        "	nt <time-specification> <notification>\n" \
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
        "	nt :5 '00:05 up'"

#define UNWANTEDATWARNLINE              "warning: commands will be executed using /bin/sh\n"

#define COLORID                         "\033[32m"
#define COLORTM                         "\033[33m"
#define COLORDF                         "\033[0m"

#define ISDIGIT(X)                      (X >= '0' && X <= '9')

/* last special token in time specification */
enum { None, Period, Comma, Second, Minute, Hour };

/* the following function assumes size >= 1 */
int
setntenv(int size, char *array[])
{
        char *c;
        char *buf;
        int len[size];
        int sumlen = 0;

        for (int i = 0; i < size; i++)
                sumlen += len[i] = strlen(array[i]);
        if (!sumlen)
                return 0;
        if (!(buf = malloc(sumlen + size))) {
                perror("setntenv - malloc");
                exit(1);
        }
        c = buf;
        memcpy(c, array[0], len[0]);
        c += len[0];
        for (int i = 1; i < size; c += len[i], i++) {
                *(c++) = ' ';
                memcpy(c, array[i], len[i]);
        }
        *c = '\0';
        if (setenv("NT_MESSAGE", buf, 1) == -1) {
                perror("setntenv - setenv");
                exit(1);
        }
        free(buf);
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
                        char *arg[] = { "/usr/bin/at", atarg, NULL };

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
                        execv(arg[0], arg);
                        perror("callat - child - execv");
                        _exit(127);
                }
                default:
                {
                        int f;
                        intmax_t id;
                        char line[128];
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
                                dprintf(fdw[1], "echo \"$$\" >%s\n"
                                                "t=$(( %jd - $(date +%%s) ))\n"
                                                "[ \"$t\" -gt 0 ] && sleep \"$t\"\n"
                                                "%s \"$NT_MESSAGE\"\n"
                                                "rm -f %s",
                                                        tmp, (intmax_t)t, NOTIFY, tmp);
                        } else
                                dprintf(fdw[1], "%s \"$NT_MESSAGE\"", NOTIFY);
                        close(fdw[1]);
                        if (!(stream = fdopen(fdr[0], "r"))) {
                                close(fdr[0]);
                                perror("callat - fdopen");
                                exit(1);
                        }
                        f = 1;
                        while (fgets(line, sizeof line, stream))
                                if (f && strcmp(line, UNWANTEDATWARNLINE) == 0)
                                        f = 0;
                                else if (t >= 0 && sscanf(line, "job %jd", &id) == 1) {
                                        printf("id: %s%jd%s, scheduled at: %s%s%s",
                                               COLORID, id, COLORDF, COLORTM, ctime(&t), COLORDF);
                                        t = -1;
                                } else
                                        fputs(line, stdout);
                        fclose(stream);
                        if (wait(NULL) == -1) {
                                perror("callat - wait");
                                exit(1);
                        }
                }
        }
}

int
main(int argc, char *argv[])
{
        unsigned int t;
        char *c;

        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
                puts(USAGE);
                return 0;
        }
        if (argc < 3) {
                fputs("nt: not enough arguments\n" USAGE "\n", stderr);
                return 2;
        }
        if (!setntenv(argc - 2, argv + 2)) {
                fputs("nt: notification can't be an empty string\n", stderr);
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
