#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>

static int 
build_dir(char* path, mode_t omode)
{
        struct stat sb;
        mode_t numask, oumask;
        int first, last, retval;
        char* p;

        p = path;
        oumask = 0;
        retval = 1;
        if (p[0] == '/')                /* Skip leading '/'. */
                ++p;
        for (first = 1, last = 0; !last ; ++p) {
                if (p[0] == '\0')
                        last = 1;
                else if (p[0] != '/')
                        continue;
                *p = '\0';
                if (!last && p[1] == '\0')
                        last = 1;
                if (first) {
                        /*
                         * POSIX 1003.2:
                         * For each dir operand that does not name an existing
                         * directory, effects equivalent to those caused by the
                         * following command shall occcur:
                         *
                         * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
                         *      mkdir [-m mode] dir
                         *
                         * We change the user's umask and then restore it,
                         * instead of doing chmod's.
                         */
                        oumask = umask(0);
                        numask = oumask & ~(S_IWUSR | S_IXUSR);
                        (void) umask(numask);
                        first = 0;
                }
                if (last)
                        (void) umask(oumask);
                if (mkdir(path, last ? omode : S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
                        if (errno == EEXIST || errno == EISDIR) {
                                if (stat(path, &sb) < 0) {
                                        retval = 0;
                                        break;
                                } else if (!S_ISDIR(sb.st_mode)) {
                                        if (last)
                                                errno = EEXIST;
                                        else
                                                errno = ENOTDIR;
                                        retval = 0;
                                        break;
                                }
                                if (last)
                                        retval = 2;
                        } else {
                                retval = 0;
                                break;
                        }
                }
                if (!last)
                        *p = '/';
        }
        if (!first && !last)
                (void) umask(oumask);
        return (retval);
}

int
main()
{
        char path[16];
        int i,j;

        for (i = 0; i < 512; i++) {
                for (j = 0; j < 512; j++) {
                        sprintf(path, "%03d/%03d", i, j);
                        build_dir(path, S_IRWXU | S_IRWXG);
                }
        }
        return 0;
}

