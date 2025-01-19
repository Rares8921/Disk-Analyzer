#include "TaskManager.h"
#include "ShmManager.h"
#include "EventManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h> // O_RDWR

// Preiau task-urile si le rulez
void runDaemon() {
    initEvents();
    // Mutex-ul va fi folosit pentru acces exclusiv la manipularea filelor
    initMutex();

    int tasksError = initTasks();

    if (tasksError != 0) {
        printf("Processed with error %d\n", tasksError);
        abort();
    }

    while (1) {
        updateTasks();
        if (getCurrentEvent() != NULL) {
            int error = processSignal(*getCurrentEvent());
            if (error) {
                printf("Task processed with error %d\n", error);
            }
            fflush(stdout);
            resetCurrentEvent();
        }

        addTask();
        sleep(1);
    }
}

// Adaug log-uri pentru semnalele primite
void handleSignal(int signal) {
    switch (signal) {
        case SIGCHLD:
        case SIGHUP:
            syslog(LOG_INFO, "Received signal %d, ignoring it.", signal);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal: %d", signal);
            break;
    }
}


//https://stackoverflow.com/questions/5384168/how-to-make-a-daemon-process
// cream un nou proces, il facem lider in grupul de procese, il izolam de procesul initial(astfel nu avem zombie)
// plasam daemon-ul la root pentru a nu exista dependente si sa nu existe blocari din cauza structurii fisierelor
static void initDaemon() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Initial fork failed");
        exit(1);
    } else if (pid > 0) {
        exit(0); // Exit parent
    }

    // https://manpages.ubuntu.com/manpages/xenial/en/man2/setsid.2.html
    if (setsid() < 0) {
        perror("Failed to become session leader");
        exit(1);
    }

    // Set up signal handling using signal()
    if (signal(SIGCHLD, handleSignal) == SIG_ERR || signal(SIGHUP, handleSignal) == SIG_ERR) {
        perror("Failed to set signal handlers");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("Second fork failed");
        exit(1);
    } else if (pid > 0) {
        exit(0); // Exit second parent
    }

    umask(0); // Reset file permissions

    if (chdir("/") < 0) {
        perror("Failed to change directory to root");
        exit(1);
    }

    // Close all file descriptors
    // https://pubs.opengroup.org/onlinepubs/000095399/functions/sysconf.html
    for (int fd = sysconf(_SC_OPEN_MAX); fd >= 0; fd--) {
        close(fd);
    }

    open ("/dev/null", O_RDWR);  
    /* stdin */  
    dup (0);  
    /* stdout */  
    dup (0);  

    openlog("daemonlog", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon initialized successfully.");
}

int main() {
    saveCurrentPath();
    initDaemon();
    runDaemon();
    return 0;
}
