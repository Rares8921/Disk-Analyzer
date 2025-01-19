#include "TaskManager.h"
#include "ShmManager.h"
#include "EventManager.h"
#include "Analyzer.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

int taskCount = 0;
struct TaskInfo tasks[DA_MAX_PROCESSES];

int compareTasksBySize(const void *a, const void *b);
int compareFiles(FileHash *a, FileHash *b);
int calculateMd5(const char *filename, unsigned char *outputHash);

void updateTasks() {
    for (int i = 1; i <= taskCount; ++i) {
        int *taskInfo = get_TaskInfo(i);
        if (taskInfo != NULL)
            tasks[i].status = *taskInfo;
    }
}

void addTask() {
    int *processCounter = getNextTask();
    if (*processCounter == DA_MAX_TASKS)
        return;

    closeShm(processCounter, sizeof(*processCounter));

    int nextTaskId = -1;

    for (int i = 1; i <= taskCount; ++i) {
        if (tasks[i].status == PENDING && 
            (nextTaskId == -1 || tasks[i].priority > tasks[nextTaskId].priority)) {
            nextTaskId = i;
        }
    }

    if (nextTaskId == -1) {
        return;
    }

    puts("found new task");
    pid_t pid = fork();

    if (pid < 0) {
        perror("couldn't fork child");
        return;
    }

    if (pid == 0) {
        processCounter = getNextTask();
        int *taskInfo = get_TaskInfo(nextTaskId);
        *taskInfo = PROCESSING;

        (*processCounter)++;
        printf("%s, %d\n", tasks[nextTaskId].path, nextTaskId);

        analyze(tasks[nextTaskId].path, nextTaskId);
        (*processCounter)--;

        *taskInfo = DONE;
        closeShm(taskInfo, sizeof(*taskInfo) * getpagesize());
        closeShm(processCounter, sizeof(*processCounter));
        exit(0);
    } else {
        tasks[nextTaskId].status = PROCESSING;
        tasks[nextTaskId].worker_pid = pid;
    }
}

int processSignal(struct SignalInfo signal) {
    switch (signal.type) {
        case ADD: {
            char output[1024];
            for (int i = 1; i <= taskCount; ++i) {
                if (tasks[i].status == REMOVED) continue;
                if (startsWith(tasks[i].path, signal.path)) {
                    sprintf(output, "Directory %s is already included in analysis with ID %d",
                            signal.path, i);
                    writeDaemonStatus(output);
                    sendEvent(signal.ppid);
                    return 0;
                }
            }

            ++taskCount;

            tasks[taskCount].task_id = taskCount;
            strcpy(tasks[taskCount].path, signal.path);
            tasks[taskCount].status = PENDING;
            tasks[taskCount].priority = signal.priority;
            tasks[taskCount].worker_pid = -1;
            tasks[taskCount].directory_size = directorySize(tasks[taskCount].path);
            sprintf(output, "%lld\n", tasks[taskCount].directory_size);

            int *taskInfo = get_TaskInfo(taskCount);
            *taskInfo = PENDING;
            closeShm(taskInfo, (int)sizeof(*taskInfo) * getpagesize());

            sprintf(output, "Created analysis task with ID %d for %s and priority %s",
                    taskCount, tasks[taskCount].path, getTaskPriority(tasks[taskCount].priority));

            char *statusPath = getCurrentPath();
            strcat(statusPath, DA_STATUS_PATH);
            sprintf(statusPath, statusPath, taskCount);
            remove(statusPath);

            FILE *statusFd = safeFileOpen(statusPath, "w");
            fprintf(statusFd, "%d%%\n%d files\n%d dirs", 0, 0, 0);
            safeFileClose(statusFd);

            writeDaemonStatus(output);
            sendEvent(signal.ppid);
            return 0;
        }

        case SUSPEND:
        case RESUME:
        case KILL: {
            int worker_pid = tasks[signal.pid].worker_pid;
            int taskStatus = tasks[signal.pid].status;

            char output[1024];
            if (worker_pid != -1 && signal.pid <= taskCount) {
                int *taskInfo = get_TaskInfo(signal.pid);
                if (signal.type == SUSPEND && taskStatus != DONE) {
                    kill(worker_pid, SIGSTOP);
                    *taskInfo = PAUSED;
                    sprintf(output, "Task with ID %d suspended", signal.pid);
                } else if (signal.type == RESUME && taskStatus != DONE) {
                    kill(worker_pid, SIGCONT);
                    *taskInfo = PROCESSING;
                    sprintf(output, "Task with ID %d resumed", signal.pid);
                } else if (signal.type == KILL) {
                    kill(worker_pid, SIGTERM);
                    *taskInfo = REMOVED;
                    sprintf(output, "Removed analysis task with ID %d for %s",
                            signal.pid, tasks[signal.pid].path);
                }
                closeShm(taskInfo, sizeof(*taskInfo) * getpagesize());
            } else {
                sprintf(output, "Task with ID %d is not running, or was never created.", signal.pid);
            }

            if (signal.type == KILL) {
                char *analysisPath = getCurrentPath();
                strcat(analysisPath, DA_ANALYSIS_PATH);
                sprintf(analysisPath, analysisPath, signal.pid);

                char *statusPath = getCurrentPath();
                strcat(statusPath, DA_STATUS_PATH);
                sprintf(statusPath, statusPath, signal.pid);

                remove(analysisPath);
                remove(statusPath);

                free(analysisPath);
                free(statusPath);
            }

            writeDaemonStatus(output);
            sendEvent(signal.ppid);
            return 0;
        }

        case INFO: {
            char output[1024];
            if (signal.pid <= taskCount && tasks[signal.pid].status != REMOVED) {
                char *statusPath = getCurrentPath();
                strcat(statusPath, DA_STATUS_PATH);
                sprintf(statusPath, statusPath, signal.pid);

                if (access(statusPath, F_OK) != 0) {
                    sprintf(output, "Task with ID %d has not yet been loaded!", signal.pid);
                    writeDaemonStatus(output);
                    sendEvent(signal.ppid);
                    return 0;
                }

                FILE *fd = safeFileOpen(statusPath, "r");

                int files, dirs, percentage;
                fscanf(fd, "%d%%\n%d files\n%d dirs", &percentage, &files, &dirs);
                safeFileClose(fd);

                char pri[] = "***";
                pri[tasks[signal.pid].priority] = '\0';

                sprintf(output, "ID  PRI PATH  DONE  STATUS  DETAILS\n%d  "
                                "%s  %s  %d%%  %s  %d files, %d dirs",
                        signal.pid, pri, tasks[signal.pid].path, percentage,
                        getTaskStatus(tasks[signal.pid].status), files, dirs);
                free(statusPath);
            } else {
                sprintf(output, "Task with ID %d does not exist.", signal.pid);
            }
            writeDaemonStatus(output);
            sendEvent(signal.ppid);
            return 0;
        }
		
		case LIST: {
			char output[4096] = "";
			snprintf(output, sizeof(output), "ID  PRI  PATH  Done  Status  Details\n");

			for (int i = 1; i <= taskCount; ++i) {
				if (tasks[i].status == REMOVED) continue;

				char *status_path = getCurrentPath();
				strcat(status_path, DA_STATUS_PATH);
				sprintf(status_path, status_path, i);

				if (access(status_path, F_OK) != 0)
					continue;

				FILE *fd = safeFileOpen(status_path, "r");

				int files, dirs, percentage;
				fscanf(fd, "%d%%\n%d files\n%d dirs", &percentage, &files, &dirs);
				safeFileClose(fd);

				char pri[] = "***";
				pri[tasks[i].priority] = '\0';

				snprintf(output + strlen(output), sizeof(output) - strlen(output),
						"%d  %s  %s  %d%%  %s  %d files, %d dirs\n",
						i, pri, tasks[i].path, percentage,
						getTaskStatus(tasks[i].status), files, dirs);

				free(status_path);
			}

			writeDaemonStatus(output);
			sendEvent(signal.ppid);
			return 0;
		}
		
		case PRINT: {
			size_t len = 256;
			char output[4096] = "";
			char *aux = calloc(256, sizeof(*aux));
			if (tasks[signal.pid].status == DONE) {

				char *analysis_path = getCurrentPath();
				strcat(analysis_path, DA_ANALYSIS_PATH);
				sprintf(analysis_path, analysis_path, signal.pid);


				FILE *fd = safeFileOpen(analysis_path, "r");
				if (fd) {
					while (1) {
						if (getline(&aux, &len, fd) == -1) break;
						if (strlen(aux) > 4096 - strlen(output)) break;
						snprintf(output + strlen(output), 4096 - strlen(output), "%s", aux);
					}
				} else {
					sprintf(output, "No existing analysis for task with ID %d", signal.pid);
				}
				safeFileClose(fd);
				free(analysis_path);
			} else {
				sprintf(output, "No existing analysis for task with ID %d", signal.pid);
			}
			free(aux);

			writeDaemonStatus(output);
			sendEvent(signal.ppid);
			return 0;
		}
		
		case EXPORT: {
			char output[1024] = "";
			char filePath[256];
			char analysisPath[256];
			int found = 0;

			for (int i = 1; i <= taskCount; i++) {
				snprintf(filePath, sizeof(filePath), "/home/rares/ProiectSO/DaemonData/Status_%d", i);
				FILE *statusFile = fopen(filePath, "r");
				if (!statusFile) {
					continue;
				}

				char firstLine[256];
				if (fgets(firstLine, sizeof(firstLine), statusFile)) {
					if (strncmp(firstLine, "100%", 4) == 0) { // Dacă analiza s-a încheiat
						snprintf(analysisPath, sizeof(analysisPath), "/home/rares/ProiectSO/DaemonData/Analysis_%d", i);
						char *data = readFromFile(analysisPath);
						if (data) {
							char exportPath[256];
							snprintf(exportPath, sizeof(exportPath), "/home/rares/ProiectSO/Exports/ExportAnalysis_%d", i);
							writeToFile(exportPath, data);
							sprintf(output, "Analysis %d exported to %s\n", i, exportPath);
							writeDaemonStatus(output);
							free(data);
							found = 1;
						} else {
							sprintf(output, "Error: Missing or unreadable file Analysis_%d for completed Status_%d\n", i, i);
							writeDaemonStatus(output);
						}
					}
				}
				fclose(statusFile);
			}

			if (!found) {
				sprintf(output, "No completed analyses found for export.\n");
				writeDaemonStatus(output);
			}

			sendEvent(signal.ppid);
			return 0;
		}
		
		case BACKUP: {
			struct TaskInfo *targetTask = NULL;
			int task_id = signal.pid;
			char output[1024] = "";

			for (int i = 1; i <= taskCount; i++) {
				if (tasks[i].task_id == task_id) {
					targetTask = &tasks[i];
					break;
				}
			}

			if (!targetTask) {
				sprintf(output, "Task with ID %d not found!\n", task_id);
				writeDaemonStatus(output);
				sendEvent(signal.ppid);
				return 0;
			}
			if (access(targetTask->path, F_OK) == -1) {
				sprintf(output, "Invalid path for task %d: %s\n", task_id, targetTask->path);
				writeDaemonStatus(output);
				sendEvent(signal.ppid);
				return 0;
			}

			createDirectoryIfNotExists(DA_BACKUP_PATH);
			if (access(DA_BACKUP_PATH, F_OK) == -1) {
				sprintf(output, "Failed to create backup directory\n");
				writeDaemonStatus(output);
				sendEvent(signal.ppid);
				return 0;
			}

			char backupPath[512];
			snprintf(backupPath, sizeof(backupPath), "%sBackupTask_%d.tar.gz", DA_BACKUP_PATH, task_id);

			char command[1024];
			snprintf(command, sizeof(command), "tar -czf %s -C %s .", backupPath, targetTask->path);

			int result = system(command);
			if (result == 0) {
				sprintf(output, "Backup for task %d created at %s\n", task_id, backupPath);
			} else {
				sprintf(output, "Failed to create backup for task %d\n", task_id);
			}

			// Trimitem output-ul
			writeDaemonStatus(output);
			sendEvent(signal.ppid);
			return 0;
		}
		
		case SORT: {
			if (taskCount <= 0 || taskCount > DA_MAX_PROCESSES) {
				writeDaemonStatus("Invalid task count.\n");
				sendEvent(signal.ppid);
				return 0;
			}

			struct TaskInfo tasksCopy[taskCount];
			int availableTasks = 0;
			for (int i = 1; i <= taskCount; i++) {
				if(tasks[i].status == REMOVED) {
					continue;
				}
				if(tasks[i].status != DONE) {
					writeDaemonStatus("All active tasks must be in done state.\n");
					sendEvent(signal.ppid);
					return 0;
				}
				tasksCopy[availableTasks++] = tasks[i];
			}

			qsort(tasksCopy, availableTasks, sizeof(struct TaskInfo), compareTasksBySize);

			char output[4096] = "";
			snprintf(output, sizeof(output), "Task ID | Folder Path                    | Size (bytes)\n");
			snprintf(output + strlen(output), sizeof(output) - strlen(output), "---------------------------------------------------------\n");

			for (int i = 0; i < availableTasks; i++) {
				snprintf(output + strlen(output), sizeof(output) - strlen(output), 
						"%7d | %-30s | %ld\n", tasksCopy[i].task_id, tasksCopy[i].path, tasksCopy[i].directory_size);
			}

			writeDaemonStatus(output);
			sendEvent(signal.ppid);
			return 0;
		}
		
		case DUPLICATE: {
			char output[4096] = ""; // Buffer pentru output
			for (int task_idx = 1; task_idx <= taskCount; ++task_idx) { // Bucla pentru task-uri
				if (tasks[task_idx].status == DONE) {
					const char *dirpath = tasks[task_idx].path;
					DIR *dir = opendir(dirpath);
					struct dirent *entry;
					struct stat file_stat;

					if (!dir) {
						snprintf(output + strlen(output), sizeof(output) - strlen(output), "Error: Could not open directory %s\n", dirpath);
						continue;
					}

					FileHash *files = malloc(sizeof(FileHash) * 1000);
					int file_count = 0;

					while ((entry = readdir(dir)) != NULL) {
						char fullpath[1024];
						snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

						if (stat(fullpath, &file_stat) == 0) {
							if (S_ISREG(file_stat.st_mode)) {
								unsigned char hash[MD5_DIGEST_LENGTH];
								if (calculateMd5(fullpath, hash) == 0) {
									// convert hash in hexa
									FileHash new_file;
									for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
										sprintf(new_file.hash + i * 2, "%02x", hash[i]);
									}
									new_file.hash[32] = '\0';
									strcpy(new_file.filepath, fullpath);
									files[file_count++] = new_file;
								} else {
									snprintf(output + strlen(output), sizeof(output) - strlen(output), "Error: Could not calculate hash for file %s\n", fullpath);
								}
							}
						}
					}

					closedir(dir);
					qsort(files, file_count, sizeof(FileHash),(int (*)(const void *, const void *))compareFiles);
					int found_duplicates = 0;
					for (int file_idx = 1; file_idx < file_count; file_idx++) {
						if (strcmp(files[file_idx].hash, files[file_idx - 1].hash) == 0) {
							if (!found_duplicates) {
								snprintf(output + strlen(output), sizeof(output) - strlen(output),
										"Duplicates found in %s:\n", tasks[task_idx].path);
								found_duplicates = 1;
							}
							snprintf(output + strlen(output), sizeof(output) - strlen(output),
									"  %s\n  %s\n\n", files[file_idx - 1].filepath, files[file_idx].filepath);
						}
					}

					if (!found_duplicates) {
						snprintf(output + strlen(output), sizeof(output) - strlen(output),
								"No duplicates found in %s.\n", tasks[task_idx].path);
					}

					free(files);
				}
			}

			if (strlen(output) == 0) {
				snprintf(output, sizeof(output), "No completed tasks to check for duplicates.\n");
			}

			writeDaemonStatus(output);
			sendEvent(signal.ppid);
			return 0;
		}
    }

    return 0;
}

void finishAllTasks() {
    for (int i = 1; i <= taskCount; ++i) {
        tasks[i].status = REMOVED;
        if (tasks[i].worker_pid != -1)
            kill(tasks[i].worker_pid, SIGTERM);
    }
}

int compareTasksBySize(const void *a, const void *b) {
    const struct TaskInfo *taskA = (const struct TaskInfo *)a;
    const struct TaskInfo *taskB = (const struct TaskInfo *)b;
    return (taskB->directory_size - taskA->directory_size);
}

int compareFiles(FileHash *a, FileHash *b) {
    return strcmp(a->hash, b->hash);
}

int calculateMd5(const char *filename, unsigned char *outputHash) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file for MD5 calculation");
        return -1;
    }

    MD5_CTX md5Ctx;
    if (MD5_Init(&md5Ctx) != 1) {
        perror("Error initializing MD5 context");
        fclose(file);
        return -1;
    }

    unsigned char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (MD5_Update(&md5Ctx, buffer, bytesRead) != 1) {
            perror("Error updating MD5 context");
            fclose(file);
            return -1;
        }
    }

    if (MD5_Final(outputHash, &md5Ctx) != 1) {
        perror("Error finalizing MD5 hash");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
