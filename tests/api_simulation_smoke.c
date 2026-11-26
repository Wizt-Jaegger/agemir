#define _POSIX_C_SOURCE 200809L
#include "../agemir_api.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct WorkerRequest {
    AgemirRuntime *runtime;
    atomic_int done;
    int succeeded;
    char error[256];
} WorkerRequest;

static void *run_request(void *data) {
    WorkerRequest *worker = data;
    AgemirRequest request = {0};
    request.operation = AGEMIR_RUN_PROJECT;
    request.project_path = "api-simulation-project";
    request.model = "simulation-smoke";
    worker->succeeded = agemir_runtime_execute(worker->runtime, &request,
                                                worker->error, sizeof(worker->error));
    atomic_store_explicit(&worker->done, 1, memory_order_release);
    return NULL;
}

int main(void) {
    AgemirRuntime *runtime = agemir_runtime_create();
    assert(runtime);
    assert(system("mkdir -p api-simulation-project") == 0);

    FILE *ledger = fopen("api-simulation-project/tasks.txt", "w");
    assert(ledger);
    fputs("[ ]\t\t\tsleep 0.5; printf done > worker-result.txt\t\tCommand progress smoke\n", ledger);
    fclose(ledger);

    WorkerRequest worker = {0};
    worker.runtime = runtime;
    atomic_init(&worker.done, 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, run_request, &worker) == 0);

    int saw_running = 0;
    AgemirStatus status = {0};
    const struct timespec interval = {0, 10000000};
    for (int i = 0; i < 500 && !atomic_load_explicit(&worker.done, memory_order_acquire); i++) {
        if (agemir_runtime_get_status(runtime, "api-simulation-project", &status) &&
            strcmp(status.state, "running") == 0 &&
            strcmp(status.current_task, "Command progress smoke") == 0 &&
            strcmp(status.current_command, "sleep 0.5; printf done > worker-result.txt") == 0 &&
            status.total_tasks == 1 && status.completed_tasks == 0 &&
            status.retry_attempt == 1 && !status.pause_requested &&
            !status.paused && !status.complete && !status.failed) {
            saw_running = 1;
        }
        nanosleep(&interval, NULL);
    }
    assert(atomic_load_explicit(&worker.done, memory_order_acquire));
    assert(pthread_join(thread, NULL) == 0);
    assert(worker.succeeded);
    assert(worker.error[0] == '\0');
    assert(saw_running);
    assert(agemir_runtime_get_status(runtime, "api-simulation-project", &status));
    assert(strcmp(status.state, "finished") == 0);
    assert(strcmp(status.phase, "completed") == 0);
    assert(status.total_tasks == 1);
    assert(status.completed_tasks == 1);
    assert(status.complete && !status.failed && !status.paused);

    FILE *result = fopen("api-simulation-project/worker-result.txt", "r");
    assert(result);
    char text[16] = "";
    assert(fgets(text, sizeof(text), result));
    fclose(result);
    assert(strcmp(text, "done") == 0);

    agemir_runtime_destroy(runtime);
    puts("api simulation worker smoke: passed");
    return 0;
}
