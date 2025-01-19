#include <stdio.h>    // fopen, fprintf, fscanf, printf
#include <stdlib.h>   // atoi, malloc, exit
#include <string.h>   // strcmp, strncpy, snprintf
#include <unistd.h>   // (kill, sleep, getpid)
#include <signal.h>   // SIGUSR1, SIGUSR2, SIGTERM
#include <sys/types.h> // pid_t

#define DA_PID_PATH "/home/rares/ProiectSO/DaemonData/DaemonPID"
#define DA_ANSWER_PATH "/home/rares/ProiectSO/DaemonData/DaemonStatus.txt"
#define DA_INSTRUCTION_PATH "/home/rares/ProiectSO/DaemonData/DaemonEvent.txt"

enum TaskList {
    ADD = 1,
    SUSPEND,
    RESUME,
    KILL,
    INFO,
    LIST,
    PRINT,
    EXPORT,
    BACKUP,
    SORT,
    DUPLICATE,
};

int daemonPID, processPID;

int getFileSize(FILE *file) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return size;
}

void writeToFile(const char *path, const char *data) {
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("Open with w flag failed on output file!");
    } else {
        fprintf(file, "%s", data);
        fclose(file);
    }
}

char* readFromFile(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Open with r flag failed on input file");
        return NULL;
    }

    int fileSize = getFileSize(file);
    char *data = malloc(fileSize);
    fread(data, fileSize, 1, file);
    fclose(file);

    return data;
}

pid_t getDaemonPID() {
    FILE *file = fopen(DA_PID_PATH, "r");
    if (!file) {
        fprintf(stderr, "PID path is empty, check configurations!\n");
        exit(EXIT_FAILURE);
    }

    int size = getFileSize(file);
    char data[size];
    fscanf(file, "%s", data);
    return atoi(data);
}

int optionalFlagEquals(const char *option, const char *str1, const char *str2) {
    return strcmp(option, str1) == 0 || strcmp(option, str2) == 0;
}

void printDaemonOutput(int signal) {
    char *data = readFromFile(DA_ANSWER_PATH);
    printf("%s\n", data);
}

void sendDaemonInstruction(const char *instruction) {
    writeToFile(DA_INSTRUCTION_PATH, instruction);
    kill(daemonPID, SIGUSR1);
    sleep(5);
}

void initializeEnviroment() {
    daemonPID = getDaemonPID();
    processPID = getpid();
    signal(SIGUSR2, printDaemonOutput);
}

void generateInstruction(int type, int pid, char *instructions) {
    snprintf(instructions, 2048, "TYPE %d\nPID %d\nPPID %d", type, pid, processPID);
}

int main(int argc, char **argv) {
    initializeEnviroment();

    if (argc == 1) {
        printf("No arguments given. Use -h or --help\n");
        return 0;
    }

    char instructions[2048];
    const char *command = argv[1];

    switch (command[1]) {
        case 'h': // -h sau --help
            if ((strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0) && argc == 2) {
                printf("Usage: da [OPTION]... [DIR]...\n"
                       "Analyze the space occupied by the directory at [DIR]\n\n"
                       "-a, --add           analyze a new directory path for disk usage\n"
                       "-p, --priority      set priority for the new analysis (works only with -a argument)\n"
                       "-S, --suspend <id>  suspend task with <id>\n"
                       "-R, --resume <id>   resume task with <id>\n"
                       "-r, --remove <id>   remove the analysis with the given <id>\n"
                       "-i, --info <id>     print status about the analysis with <id> (pending, progress, done)\n"
                       "-l, --list          list all analysis tasks, with their ID and the corresponding root path\n"
                       "-p, --print <id>    print analysis report for those tasks that are \"done\"\n"
                       "-e, --export        export analysis result in a text file\n"
                       "-b, --backup <id>   creates a backup of content specified at analysis with the given <id>\n"
                       "-s, --sort          sort folders from all tasks decreasing according to the size of the files\n"
                       "-d, --dupl          lists the duplicates inside all folders specified at analysis\n"
                       "-t, --terminate     terminates the daemon analysis\n");
            } else {
                printf("Unknown command! Use -h or --help for usage information.\n");
            }
            break;

        case 'a': // -a sau --add
            if ((strcmp(command, "-a") == 0 || strcmp(command, "--add") == 0) && (argc == 3 || argc == 5)) {
                if (argc < 3) {
                    printf("Not enough arguments provided. Exiting...\n");
                    return -1;
                }
                int priority = 1;
                char *path = argv[2];
                if (argc == 5 && optionalFlagEquals(argv[3], "-p", "--priority")) {
                    priority = atoi(argv[4]);
                    if (priority < 1 || priority > 3) {
                        printf("Priority should have one of the values: 1-low, 2-normal, 3-high\n");
                        return -1;
                    }
                }
                snprintf(instructions, 2048, "TYPE %d\nPRIORITY %d\nPATH %s\nPPID %d", ADD, priority, path, processPID);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -a. Use -h or --help for usage information.\n");
            }
            break;

        case 'S': // -S sau --suspend
            if ((strcmp(command, "-S") == 0 || strcmp(command, "--suspend") == 0) && argc == 3) {
                if (argc < 3) {
                    printf("Not enough arguments provided. Exiting...\n");
                    return -1;
                }
                int pid = atoi(argv[2]);
                generateInstruction(SUSPEND, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -S. Use -h or --help for usage information.\n");
            }
            break;

        case 'R': // -R sau --resume
            if ((strcmp(command, "-R") == 0 || strcmp(command, "--resume") == 0) && argc == 3) {
                if (argc < 3) {
                    printf("Not enough arguments provided. Exiting...\n");
                    return -1;
                }
                int pid = atoi(argv[2]);
                generateInstruction(RESUME, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -R. Use -h or --help for usage information.\n");
            }
            break;

        case 'r': // -r sau --remove
            if ((strcmp(command, "-r") == 0 || strcmp(command, "--remove") == 0) && argc == 3) {
                if (argc < 3) {
                    printf("Not enough arguments provided. Exiting...\n");
                    return -1;
                }
                int pid = atoi(argv[2]);
                generateInstruction(KILL, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -r. Use -h or --help for usage information.\n");
            }
            break;

        case 'i': // -i sau --info
            if ((strcmp(command, "-i") == 0 || strcmp(command, "--info") == 0) && argc == 3) {
                if (argc < 3) {
                    printf("Not enough arguments provided. Exiting...\n");
                    return -1;
                }
                int pid = atoi(argv[2]);
                generateInstruction(INFO, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -i. Use -h or --help for usage information.\n");
            }
            break;

        case 'l': // -l sau --list
            if ((strcmp(command, "-l") == 0 || strcmp(command, "--list") == 0) && argc == 2) {
                generateInstruction(LIST, 0, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -l. Use -h or --help for usage information.\n");
            }
            break;

        case 'p': // -p sau --print
            if ((strcmp(command, "-p") == 0 || strcmp(command, "--print") == 0) && argc == 3) {
                int pid = atoi(argv[2]);
                generateInstruction(PRINT, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid usage of -p. Correct format: -p <id>.\n");
            }
            break;

        case 'e': // -e sau --export
            if ((strcmp(command, "-e") == 0 || strcmp(command, "--export") == 0) && argc == 2) {
                generateInstruction(EXPORT, -1, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid usage of -e. This command takes no additional arguments.\n");
            }
            break;

        case 'b': // -b sau --backup
            if ((strcmp(command, "-b") == 0 || strcmp(command, "--backup") == 0) && argc == 3) {
                int pid = atoi(argv[2]);
                generateInstruction(BACKUP, pid, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid usage of -b. Correct format: -b <id>.\n");
            }
            break;

        case 's': // -s sau --sort
            if ((strcmp(command, "-s") == 0 || strcmp(command, "--sort") == 0) && argc == 2) {
                generateInstruction(SORT, -2, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid usage of -s. This command takes no additional arguments.\n");
            }
            break;

        case 'd': // -d sau --dupl
            if ((strcmp(command, "-d") == 0 || strcmp(command, "--dupl") == 0) && argc == 2) {
                generateInstruction(DUPLICATE, -3, instructions);
                sendDaemonInstruction(instructions);
            } else {
                printf("Invalid use of -d. Use -h or --help for usage information.\n");
            }
            break;

        case 't': // -t sau --terminate
            if ((strcmp(command, "-t") == 0 || strcmp(command, "--terminate") == 0) && argc == 2) {
                FILE *fp = popen("ps aux | grep '[d]aemon' | awk '{print $2}'", "r");
                if (fp == NULL) {
                    perror("popen");
                    break;
                }

                char pid_str[10];
                while (fgets(pid_str, sizeof(pid_str), fp) != NULL) {
                    pid_t pid = atoi(pid_str);
                    if (pid > 0) {
                        if (kill(pid, SIGKILL) == 0) {
                            printf("Successfully terminated daemon process with PID %d.\n", pid);
                        } else {
                            perror("kill");
                        }
                    }
                }

                fclose(fp);
            } else {
                printf("Invalid use of -t. Use -h or --help for usage information.\n");
            }
            break;

        default:
            printf("This command does not exist! Use -h or --help\n");
            break;
    }

    return 0;
}
