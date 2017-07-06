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

void mkvfat(const char *device)
{
}

int e2fscheck(const char *device)
{
    return run_prog_and_wait("/system/bin/e2fsck", "-f", "-y", device, NULL);
}

#define PROG_MKE2FS "/system/bin/mke2fs"

void mke2fs(const char *device)
{
    run_prog_and_wait(PROG_MKE2FS, "-T", "ext4", device, NULL);
}

void makefs(const char *device, const char *type)
{
    if (!type)
        type = "ext4";

    if (strncmp(type, "ext4", 4) == 0)
        mke2fs(device);
    else if (strncmp(type, "vfat", 4) == 0)
        mkvfat(device);
}

typedef void (*callback_t)(const char *alias, const char *device);

void setupfs(const char *alias, const char *device)
{
    char linkpath[64], path[64];
    int ret, fs_format = 0, fs_check = 0;
    char *fstype = "ext4";
    int status;

    snprintf(path, sizeof(path), "/dev/block/%s", device);
    snprintf(linkpath, sizeof(linkpath), "/dev/block/%s", alias);

    if (strncmp(alias, "data", 4) == 0) {
        fs_check = 1;
    } else if (strncmp(alias, "cache", 5) == 0) {
        fs_check = 1;
    } else if (strncmp(alias, "misc", 4) == 0) {
    } else if (strncmp(alias, "UDISK", 5) == 0) {
        // format to VFAT
        fstype = "vfat";
    }

    if (fs_check) {
        status = e2fscheck(path);
        if (status == 8) {
            // operational error, usually caused by unknown file system
            fs_format = 1;
        }
    }

    if (fs_format) {
        makefs(path, "ext4");
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
