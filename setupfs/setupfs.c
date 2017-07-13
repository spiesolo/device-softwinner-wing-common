#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <sys/wait.h>

#ifndef TEMP_FAILURE_RETRY
# define TEMP_FAILURE_RETRY(expression) \
      ({ long int __result; \
        do __result = (long int) (expression); \
        while (__result == -1L && errno == EINTR); \
        __result; })
#endif

#define MAX_ARGS    10

int run_prog_and_wait(const char *path, ...)
{
    int 
    status = -1;
    int pid;

    if (access(path, X_OK) == 0) {
        pid = fork();
        if (pid == 0) {
            char *argv[MAX_ARGS + 1];
            int n = 0;
            va_list ap;

            argv[n++] = path;
            va_start(ap, path);
            while (n < MAX_ARGS) {
                if ((argv[n++] = va_arg(ap, char *)) == NULL)
                    break;
            }
            va_end(ap);
            argv[n] = NULL;
            execv(path, argv);
            _exit(127);
        } else if (pid > 0) {
            waitpid(pid, &status, 0);
        }
    }

    return WEXITSTATUS(status);
}

static char FSCK_MSDOS_PATH[] = "/system/bin/fsck_msdos";

static char MKDOSFS_PATH[] = "/system/bin/newfs_msdos";

int fatcheck(const char *device)
{
    int pass = 1;

    do {
        int status = run_prog_and_wait(FSCK_MSDOS_PATH, "-p", device, NULL);
        switch (status) {
        case 0: // Filesystem check completed OK
            break;
        case 4:
            if (pass++ < 3)
                continue; // Filesystem modified - rechecking (pass %d)
            // Failing check after too many rechecks
        case 2:  // Filesystem check failed (not a FAT filesystem)
        default: // Filesystem check failed (unknown exit code)
            return -1;
        }
    } while (0);

    return 0;
}

void fatformat(const char *device, const char *alias)
{
    run_prog_and_wait(MKDOSFS_PATH, "-F", "32", "-O", "android", "-c", "8", "-L", alias, device, NULL);
}

static char E2FSCK_PATH[] = "/system/bin/e2fsck";

static char MKE2FS_PATH[] = "/system/bin/mke2fs";

int e2fscheck(const char *device)
{
    int status = run_prog_and_wait(E2FSCK_PATH, "-f", "-y", device, NULL);
    if (status == 8) {
        // operational error, usually not ext4 file system
        return -1;
    }
    return 0;
}

void e2fsformat(const char *device, const char *alias)
{
    run_prog_and_wait(MKE2FS_PATH, "-T", "ext4", device, NULL);
}

typedef int (*fscheck_t)(const char *device);
typedef void (*fsformat_t)(const char *device, const char *alias);

typedef void (*callback_t)(const char *alias, const char *device);

void setupfs(const char *alias, const char *device)
{
    char linkpath[64], path[64];
    int ret, fs_format = 0, fs_check = 0;
    fscheck_t fscheck = NULL;
    fsformat_t fsformat = NULL;
    int status;

    snprintf(path, sizeof(path), "/dev/block/%s", device);
    snprintf(linkpath, sizeof(linkpath), "/dev/block/%s", alias);

    if (strncmp(alias, "data", 4) == 0) {
        fscheck = e2fscheck;
        fsformat = e2fsformat;
    } else if (strncmp(alias, "cache", 5) == 0) {
        fscheck = e2fscheck;
        fsformat = e2fsformat;
    } else if (strncmp(alias, "misc", 4) == 0) {
    } else if (strncmp(alias, "UDISK", 5) == 0) {
        fscheck = fatcheck;
        fsformat = fatformat;
    }

    if (fscheck) {
        status = fscheck(path);
        if (status) {
            if (fsformat)
                fsformat(path, alias);
        }
    }

    // finally create link
    ret = symlink(path, linkpath);
    if (ret < 0) {
        printf("failed to create symlink: %s\n", strerror(errno));
    }
}

int do_parse_cmdline(const char *path, callback_t fn)
{
    char buffer[1024];
    char *token, *p;
    int count;
    int fd;

    if (path == NULL)
        path = "/proc/cmdline";

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    count = TEMP_FAILURE_RETRY(read(fd, buffer, sizeof(buffer) - 1));
    if (count < 0) {
        close(fd);
        return -1;
    }
    if (buffer[count - 1] == '\n')
        buffer[count - 1] = 0;
    else
        buffer[count] = 0;

    p = buffer;
    while ((token = strtok(p, " ")) != NULL) {
        if (strncmp(token, "partitions=", 11) == 0) {
            token += 11;
            break;
        }
        if (p) p = NULL;
    }

    if (token != NULL) {
        p = token;
        while ((token = strtok(p, ":")) != NULL) {
            p = strchr(token, '@');
            if (p != NULL) *p++ = 0;
            fn(token, p);
            p = NULL;
        }
    }

    return 0;
}

int main(int ac, char *av[])
{
    do_parse_cmdline(NULL, setupfs);
    return 0;
}
