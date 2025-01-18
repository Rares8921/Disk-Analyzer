#include <stdio.h>    // fopen, fprintf, fscanf, printf
#include <stdlib.h>   // atoi, malloc, exit
#include <string.h>   // strcmp, strncpy, snprintf
#include <unistd.h>   // (kill, sleep, getpid)
#include <signal.h>   // SIGUSR1, SIGUSR2, SIGTERM
#include <sys/types.h> // pid_t

#define DA_PID_PATH "de_completat"
#define DA_OUTPUT_PATH "de_completat"
#define DA_INSTRUCTION_PATH "de_completat"

const enum TaskList {
    ADD = 1,
    SUSPEND,
    RESUME,
    KILL,
    INFO,
    LIST,
    PRINT
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
    char *data = readFromFile(DA_OUTPUT_PATH);
    printf("%s\n", data);
}

void sendDaemonInstruction(const char *instruction) {
    writeToFile(DA_INSTRUCTION_PATH, instruction);
    kill(daemonPID, SIGUSR1);
    sleep(3);
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

    /**
     * TODO:
     *      --export
     *      --backup
     *      --sort
     *      --dupl
     */

    char instructions[2048];
    switch (argc > 1 ? argv[1][1] : 0) {

        case 'h': {  // -h, --help
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
                   "-e, --export        export analysis result in a text file\n\n"
                   "-b, --backup <id>   creates a backup of content specified at analysis with the given <id>\n\n"
                   "-s, --sort          sort folders from all tasks decreasing according to the size of the files\n\n"
                   "-d, --dupl <id>     lists the duplicates inside folder specified at analysis with the given <id>\n\n"
                   "-t, --terminate     terminates the daemon analysis\n\n");
            break;
        }

        case 'a': {  // -a, --add
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
            break;
        }

        case 'S': {  // -S, --suspend
            if (argc < 3) 
                return -1;
            int pid = atoi(argv[2]);
            generateInstruction(SUSPEND, pid, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'R': {  // -R, --resume
            if (argc < 3) 
                return -1;
            int pid = atoi(argv[2]);
            generateInstruction(RESUME, pid, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'r': {  // -r, --remove
            if (argc < 3) 
                return -1;
            int pid = atoi(argv[2]);
            generateInstruction(KILL, pid, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'i': {  // -i, --info
            if (argc < 3) 
                return -1;
            int pid = atoi(argv[2]);
            generateInstruction(INFO, pid, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'l': {  // -l, --list
            generateInstruction(LIST, 0, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'p': {  // -p, --print
            if (argc < 3) 
                return -1;
            int pid = atoi(argv[2]);
            generateInstruction(PRINT, pid, instructions);
            sendDaemonInstruction(instructions);
            break;
        }

        case 'e': { // -e, --export
            break;
        }

        case 'b': { // -b, --backup
            break;
        }

        case 's': { // -s, --sort
            break;
        }

        case 'd': { // -d, --dupl
            break;
        }

        case 't': {  // -t, --terminate
            pid_t receivedPID = getDaemonPID();
            if (receivedPID != -1) {
                kill(receivedPID, SIGTERM);
                printf("The daemon with PID `%d` terminated successfully!\n", receivedPID);
            }
            break;
        }

        default:
            printf("This command does not exist! Use -h or --help\n");
            break;
    }

    return 0;
}