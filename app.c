#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define maxgrp 30        // Maximum number of groups
#define maxlen 256       // Maximum length for file paths

struct msg_buffer {
    long mtype;         // Message type
    char mtext[256];    // Message text
    int modifyingGroup; // Group number that terminated
};

int main(int cou, char *stra[])
{
    struct msg_buffer rec_msg;     // Variable to hold the received message from the message queue
    FILE *inpf;                    // File pointer for "input.txt"
    int ngrp;                      // Number of groups to be created (read from input.txt)
    int k_app;                     // Key for the message queue between groups.c and app.c (read from input.txt)
    int mqid;                      // Message queue identifier
    int fg = 0;                    // Counter for finished (terminated) groups
    char grp_fp[maxgrp][maxlen];   // Array to store group file paths from input.txt
    pid_t grppid[maxgrp];          // Array to store process IDs of the child group processes

    inpf = fopen("input.txt", "r");  // Open input.txt for reading
    if (inpf == NULL) {
        printf("Error: input.txt can't be opened \n");
        exit(1);
    }

    fscanf(inpf, "%d", &ngrp);       // Read the number of groups

    fscanf(inpf, "%*d");            // Skip the key for groups.c and validation.out
    fscanf(inpf, "%d", &k_app);      // Read the key for the message queue between groups.c and app.c
    fscanf(inpf, "%*d");            // Skip the key for groups.c and moderator.c
    fscanf(inpf, "%*d");            // Skip the violations threshold

    for (int i = 0; i < ngrp; i++) { // Read each group file path into grp_fp array
        fscanf(inpf, "%s", grp_fp[i]);
    }
    fclose(inpf);                  // Close the input file

    mqid = msgget(k_app, IPC_CREAT | 0644);  // Create or attach to the message queue using k_app
    if (mqid <= 0) {
        printf("Error: message queue creation or attachment \n");
        exit(1);
    }

    for (int i = 0; i < ngrp; i++) {  // For each group, fork a new process
        pid_t pid = fork();          // Create a new process
        if (pid < 0) {
            printf("Error: Couldn't fork\n");
            exit(1);
        }

        if (pid == 0) {              // Child process block
            char testc[10];        // Buffer to hold the test case argument
            if (cou > 1) {         // If a command-line argument is provided beyond the program name
                strncpy(testc, stra[1], sizeof(testc) - 1); // Copy the first argument into testc safely
                testc[sizeof(testc) - 1] = '\0';             // Ensure null-termination
            }
            else {
                strcpy(testc, "0"); // Default value "0" if no extra argument is provided
            }
            execl("./groups.out", "./groups.out", testc, grp_fp[i], (char*)NULL); // Replace the child process with groups.out
            printf("Error: Execl failed\n");  // If execl fails, print an error
            exit(1);                          // Exit the child process
        }
        else {                       // Parent process block
            grppid[i] = pid;       // Store the child process ID in grppid array
        }
    }

    while (fg < ngrp) {             // Loop until termination messages have been received for all groups
        if (msgrcv(mqid, &rec_msg, sizeof(rec_msg) - sizeof(long), 100, 0) == -1) {
            printf("Error: Can't receive message\n");
        }
        else {
            printf("Group %d terminated.\n", rec_msg.modifyingGroup);  // Print termination message for the group
            fg++;                   // Increment the count of terminated groups
        }
    }

    for (int i = 0; i < ngrp; i++) {  // Wait for all child processes to finish
        waitpid(grppid[i], NULL, 0);
    }
    return 0;
}

