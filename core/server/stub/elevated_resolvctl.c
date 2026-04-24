#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <grp.h>
#include <limits.h>

int main(int argc, char *argv[]) {
    char proc_path[PATH_MAX];
    char exe_path[PATH_MAX];
    struct stat st;
    struct group *grp;

    // Get the path to the parent process's executable
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", getppid());

    // realpath() resolves the symlink
    if (realpath(proc_path, exe_path) == NULL) {
        printf("realpath not called \n");
        printf("%s\n", proc_path);
        return 1;
    }

    // Get file status to find the Group ID (GID)
    if (stat(exe_path, &st) != 0) {
        printf("status not found \n");
        return 1;
    }

    // Look up the group name from the GID
    grp = getgrgid(st.st_gid);
    if (grp == NULL) {
        printf("group not found \n");
        return 1;
    }

    if (strcmp(grp->gr_name, "sing-box") != 0){
        printf("sing-box != %s", grp->gr_name);
        return 1;
    }

    {
        // Prepare arguments for resolvectl (skipping the current program name)
        char **args = malloc((argc + 1) * sizeof(char *));
        args[0] = "resolvectl";
        for (int i = 1; i < argc; i++) {
            args[i] = argv[i];
        }
        args[argc] = NULL;
        // Execute resolvectl with the passed arguments
        execvp("resolvectl", args);

        // execvp only returns if it fails
        perror("execvp");
        free(args);
        return 1;
    }
}
