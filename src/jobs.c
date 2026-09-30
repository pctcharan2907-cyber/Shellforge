#define _POSIX_C_SOURCE 200809L

#include "jobs.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#define PROCESS_RUNNING 0
#define PROCESS_STOPPED 1
#define PROCESS_DONE 2

static job_t job_table[MAX_JOBS];
static int next_job_id = 1;

static void update_job_state(job_t *job)
{
    int active = 0;
    int stopped = 0;

    for (int i = 0; i < job->process_count; i++) {
        if (job->process_states[i] == PROCESS_DONE) {
            continue;
        }

        active++;

        if (job->process_states[i] == PROCESS_STOPPED) {
            stopped++;
        }
    }

    if (active == 0 && job->process_count > 0) {
        job->state = JOB_DONE;
    } else if (active > 0 && stopped == active) {
        job->state = JOB_STOPPED;
    } else {
        job->state = JOB_RUNNING;
    }
}

void jobs_init(void)
{
    memset(job_table, 0, sizeof(job_table));
    next_job_id = 1;
}

int job_add(pid_t pgid, const char *command, job_state_t state)
{
    int slot = -1;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        fprintf(stderr, "shellforge: job table is full\n");
        return -1;
    }

    while (job_find(next_job_id) != NULL || next_job_id <= 0) {
        next_job_id++;
        if (next_job_id <= 0) {
            next_job_id = 1;
        }
    }

    job_t *job = &job_table[slot];
    memset(job, 0, sizeof(*job));

    job->job_id = next_job_id++;
    job->pgid = pgid;
    job->state = state;

    if (command != NULL) {
        snprintf(job->command, sizeof(job->command), "%s", command);
    }

    return job->job_id;
}

int job_track_process(int job_id, pid_t pid)
{
    job_t *job = job_find(job_id);

    if (job == NULL || job->process_count >= MAX_JOB_PROCESSES) {
        return -1;
    }

    int index = job->process_count++;
    job->pids[index] = pid;
    job->process_states[index] =
        (job->state == JOB_STOPPED) ? PROCESS_STOPPED : PROCESS_RUNNING;

    return 0;
}

job_t *job_find(int job_id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id == job_id) {
            return &job_table[i];
        }
    }

    return NULL;
}

job_t *job_find_by_pgid(pid_t pgid)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id != 0 && job_table[i].pgid == pgid) {
            return &job_table[i];
        }
    }

    return NULL;
}

job_t *job_latest(void)
{
    job_t *latest = NULL;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].job_id != 0 &&
            (latest == NULL || job_table[i].job_id > latest->job_id)) {
            latest = &job_table[i];
        }
    }

    return latest;
}

void job_update_process(pid_t pid, int wait_status)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *job = &job_table[i];

        if (job->job_id == 0) {
            continue;
        }

        for (int j = 0; j < job->process_count; j++) {
            if (job->pids[j] != pid) {
                continue;
            }

            if (WIFSTOPPED(wait_status)) {
                job->process_states[j] = PROCESS_STOPPED;
            } else if (WIFCONTINUED(wait_status)) {
                job->process_states[j] = PROCESS_RUNNING;
            } else if (WIFEXITED(wait_status) || WIFSIGNALED(wait_status)) {
                job->process_states[j] = PROCESS_DONE;
            }

            update_job_state(job);
            return;
        }
    }
}

void jobs_reap(void)
{
    int status;
    pid_t pid;

    for (;;) {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if (pid > 0) {
            job_update_process(pid, status);
            continue;
        }

        if (pid < 0 && errno == EINTR) {
            continue;
        }

        break;
    }
}

void job_remove(int job_id)
{
    job_t *job = job_find(job_id);

    if (job != NULL) {
        memset(job, 0, sizeof(*job));
    }
}

void jobs_print(void)
{
    jobs_reap();

    int completed_ids[MAX_JOBS];
    int completed_count = 0;

    for (int i = 0; i < MAX_JOBS; i++) {
        job_t *job = &job_table[i];

        if (job->job_id == 0) {
            continue;
        }

        const char *state_name;

        switch (job->state) {
            case JOB_RUNNING:
                state_name = "Running";
                break;
            case JOB_STOPPED:
                state_name = "Stopped";
                break;
            case JOB_DONE:
                state_name = "Done";
                completed_ids[completed_count++] = job->job_id;
                break;
            default:
                state_name = "Unknown";
                break;
        }

        printf("[%d] %-7s %s\n",
               job->job_id,
               state_name,
               job->command);
    }

    for (int i = 0; i < completed_count; i++) {
        job_remove(completed_ids[i]);
    }
}

void job_stop(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job == NULL) {
        return;
    }

    for (int i = 0; i < job->process_count; i++) {
        if (job->process_states[i] != PROCESS_DONE) {
            job->process_states[i] = PROCESS_STOPPED;
        }
    }

    job->state = JOB_STOPPED;
}

void job_continue(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job == NULL) {
        return;
    }

    for (int i = 0; i < job->process_count; i++) {
        if (job->process_states[i] == PROCESS_STOPPED) {
            job->process_states[i] = PROCESS_RUNNING;
        }
    }

    job->state = JOB_RUNNING;
}

void job_done(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job == NULL) {
        return;
    }

    for (int i = 0; i < job->process_count; i++) {
        job->process_states[i] = PROCESS_DONE;
    }

    job->state = JOB_DONE;
}
