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

    agemir_runtime_destroy(runtime);
    puts("api smoke: passed");
    return 0;
}
