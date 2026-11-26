#define _POSIX_C_SOURCE 200809L
#include "../agemir_api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_state(const char *directory) {
    char path[256];
    snprintf(path, sizeof(path), "%s/progress_state.txt", directory);
    FILE *file = fopen(path, "w");
    assert(file);
    fputs("status:running\n", file);
    fputs("task:Create native window\n", file);
    fputs("reason:Waiting for model\n", file);
    fputs("attempts:3\n", file);
    fputs("updated:2026-11-26\n", file);
    fclose(file);
}

int main(void) {
    AgemirRuntime *runtime = agemir_runtime_create();
    assert(runtime);

    char value[64] = "unchanged";
    assert(agemir_runtime_get_setting(runtime, "model", value, sizeof(value)));
    assert(value[0]);
    assert(!agemir_runtime_get_setting(runtime, "model", value, 1));
    assert(value[0] == '\0');
    assert(agemir_runtime_set_setting(runtime, "project_path", "/tmp/agemir work"));
    char project_path[512] = "";
    assert(agemir_runtime_get_setting(runtime, "project_path", project_path, sizeof(project_path)));
    assert(strcmp(project_path, "/tmp/agemir work") == 0);
    char short_path[8] = "short";
    assert(!agemir_runtime_get_setting(runtime, "project_path", short_path, sizeof(short_path)));
    assert(short_path[0] == '\0');
    char too_long_path[513];
    memset(too_long_path, 'x', sizeof(too_long_path) - 1);
    too_long_path[sizeof(too_long_path) - 1] = '\0';
    assert(!agemir_runtime_set_setting(runtime, "project_path", too_long_path));
    assert(!agemir_runtime_set_setting(runtime, "project_path", "bad\npath"));
    assert(!agemir_runtime_set_setting(runtime, "language", "0"));
    assert(agemir_runtime_set_setting(runtime, "language", "1"));
    assert(!agemir_runtime_set_setting(runtime, "unknown", "value"));

    char error[128] = "not cleared";
    AgemirRequest request = {0};
    assert(!agemir_runtime_execute(runtime, &request, error, sizeof(error)));
    assert(strcmp(error, "unsupported operation") == 0);
    assert(!agemir_runtime_execute(runtime, NULL, error, sizeof(error)));
    assert(strstr(error, "required"));

    assert(system("mkdir -p project") == 0);
    write_state("project");
    AgemirStatus status;
    assert(agemir_runtime_get_status(runtime, "project", &status));
    assert(strcmp(status.state, "running") == 0);
    assert(strcmp(status.task, "Create native window") == 0);
    assert(status.attempts == 3);
    assert(!agemir_runtime_get_status(runtime, "missing", &status));

    assert(agemir_runtime_pause(runtime, "project", error, sizeof(error)));
    FILE *pause_file = fopen("project/PAUSE", "r");
    assert(pause_file);
    fclose(pause_file);
    assert(!agemir_runtime_pause(runtime, "", error, sizeof(error)));
    assert(strstr(error, "required"));

    FILE *ledger = fopen("project/tasks.txt", "w");
    assert(ledger);
    fclose(ledger);
    const char *current_path = getenv("PATH");
    char *saved_path = strdup(current_path ? current_path : "");
    assert(saved_path);
    assert(setenv("PATH", "/agemir-test-no-commands", 1) == 0);
    request.operation = AGEMIR_RUN_PROJECT;
    request.project_path = "project";
    request.model = "test-model";
    assert(!agemir_runtime_execute(runtime, &request, error, sizeof(error)));
    assert(strstr(error, "Ollama is not installed"));
    assert(setenv("PATH", saved_path, 1) == 0);
    free(saved_path);

    agemir_runtime_destroy(runtime);
    puts("api smoke: passed");
    return 0;
}
