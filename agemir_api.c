/*
 * The legacy core currently lives in Agemir.c and its included C modules.
 * Compile this translation unit for consumers of the C API; the core is
 * compiled exactly once and the interactive CLI main is omitted.
 */
#define AGEMIR_NO_MAIN
#include "Agemir.c"

struct AgemirRuntime {
    int benchmark_tps;
};

static int copy_api_text(char *destination, size_t destination_size,
                         const char *source) {
    if (!destination || destination_size == 0 || !source) return 0;
    size_t length = strlen(source);
    if (length >= destination_size) {
        destination[0] = '\0';
        return 0;
    }
    memcpy(destination, source, length + 1);
    return 1;
}

static void set_api_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0 || !message) return;
    size_t length = strlen(message);
    if (length >= error_size) length = error_size - 1;
    memcpy(error, message, length);
    error[length] = '\0';
}

AgemirRuntime *agemir_runtime_create(void) {
    AgemirRuntime *runtime = (AgemirRuntime *)calloc(1, sizeof(*runtime));
    if (!runtime) return NULL;

    int language = config_get_int("language", 1);
    if (language < 1 || language > 5) language = 1;
    load_language(language);
    config_get("username", username, sizeof(username));
    load_model_catalog();
    runtime->benchmark_tps = config_get_int("benchmark_v2", 24);
    if (runtime->benchmark_tps < 1) runtime->benchmark_tps = 24;
    return runtime;
}

void agemir_runtime_destroy(AgemirRuntime *runtime) {
    free(runtime);
}

int agemir_runtime_get_setting(AgemirRuntime *runtime, const char *key,
                               char *value, size_t value_size) {
    if (!runtime || !key || !value || value_size == 0) return 0;
    char setting[64];
    if (strcmp(key, "username") == 0) {
        if (!config_get(key, setting, sizeof(setting))) setting[0] = '\0';
        return copy_api_text(value, value_size, setting);
    }
    if (strcmp(key, "model") == 0) {
        if (!config_get(key, setting, sizeof(setting)))
            snprintf(setting, sizeof(setting), "auto");
        return copy_api_text(value, value_size, setting);
    }
    if (strcmp(key, "language") == 0 || strcmp(key, "benchmark_v2") == 0) {
        snprintf(setting, sizeof(setting), "%d", config_get_int(key,
                 strcmp(key, "language") == 0 ? 1 : runtime->benchmark_tps));
        return copy_api_text(value, value_size, setting);
    }
    return 0;
}

int agemir_runtime_set_setting(AgemirRuntime *runtime, const char *key,
                               const char *value) {
    if (!runtime || !key || !value) return 0;
    if (strcmp(key, "username") == 0) {
        if (strlen(value) == 0 || strlen(value) >= sizeof(username) ||
            strpbrk(value, "\r\n")) return 0;
        config_set(key, value);
        snprintf(username, sizeof(username), "%s", value);
        return 1;
    }
    if (strcmp(key, "model") == 0) {
        if (!*value || strlen(value) >= 64 || strpbrk(value, "\r\n")) return 0;
        config_set(key, value);
        return 1;
    }
    if (strcmp(key, "language") == 0) {
        char *end = NULL;
        long language = strtol(value, &end, 10);
        if (!*value || !end || *end || language < 1 || language > 5) return 0;
        config_set_int(key, (int)language);
        load_language((int)language);
        return 1;
    }
    return 0;
}

int agemir_runtime_project_count(AgemirRuntime *runtime) {
    return runtime ? config_project_count() : -1;
}

int agemir_runtime_project_at(AgemirRuntime *runtime, int index,
                              AgemirProject *project) {
    if (!runtime || index < 0 || !project) return 0;
    return config_project_at(index + 1, project->name, sizeof(project->name),
                             project->path, sizeof(project->path));
}

int agemir_runtime_execute(AgemirRuntime *runtime, const AgemirRequest *request,
                           char *error, size_t error_size) {
    set_api_error(error, error_size, "");
    if (!runtime || !request) {
        set_api_error(error, error_size, "runtime and request are required");
        return 0;
    }

    char selected_model[64];
    const char *model = request->model;
    if (!model || !*model) {
        int is_auto = 0;
        current_model(runtime->benchmark_tps, total_ram_mb(), selected_model,
                      sizeof(selected_model), &is_auto);
        model = selected_model;
    }
    if (strlen(model) >= sizeof(selected_model)) {
        set_api_error(error, error_size, "model name is too long");
        return 0;
    }

    if (request->operation == AGEMIR_RUN_PROJECT) {
        if (!request->project_path || !*request->project_path) {
            set_api_error(error, error_size, "project path is required");
            return 0;
        }
        if (!run_agent(request->project_path, model)) {
            set_api_error(error, error_size, "project run could not start; see console output");
            return 0;
        }
        AgemirStatus result;
        if (agemir_runtime_get_status(runtime, request->project_path, &result) &&
            (strcmp(result.state, "failed") == 0 ||
             strcmp(result.state, "error") == 0 ||
             strcmp(result.state, "paused") == 0)) {
            set_api_error(error, error_size,
                          result.reason[0] ? result.reason : result.state);
            return 0;
        }
        return 1;
    }
    if (request->operation == AGEMIR_GENERATE_PROJECT) {
        if (!request->project_path || !*request->project_path ||
            !request->project_name || !is_valid_name(request->project_name) ||
            !request->description || !*request->description) {
            set_api_error(error, error_size,
                          "valid project path, name, and description are required");
            return 0;
        }
        if (!generate_project(model, request->project_name, request->project_path,
                              request->description, request->graphical != 0)) {
            set_api_error(error, error_size, "project generation failed; see console output");
            return 0;
        }
        return 1;
    }
    set_api_error(error, error_size, "unsupported operation");
    return 0;
}

int agemir_runtime_get_status(AgemirRuntime *runtime, const char *project_path,
                              AgemirStatus *status) {
    if (!runtime || !project_path || !*project_path || !status) return 0;
    memset(status, 0, sizeof(*status));
    char state_path[1024], attempts[16] = "";
    join_path(state_path, sizeof(state_path), project_path, STATE_NAME);
    if (!file_exists(state_path)) return 0;
    kv_get(state_path, "status", status->state, sizeof(status->state));
    kv_get(state_path, "task", status->task, sizeof(status->task));
    kv_get(state_path, "reason", status->reason, sizeof(status->reason));
    kv_get(state_path, "attempts", attempts, sizeof(attempts));
    kv_get(state_path, "updated", status->updated, sizeof(status->updated));
    status->attempts = atoi(attempts);
    return status->state[0] != '\0';
}

int agemir_runtime_pause(AgemirRuntime *runtime, const char *project_path,
                         char *error, size_t error_size) {
    if (error && error_size) error[0] = '\0';
    if (!runtime || !project_path || !*project_path) {
        if (error && error_size) snprintf(error, error_size, "project path is required");
        return 0;
    }
    char pause_path[1024];
    join_path(pause_path, sizeof(pause_path), project_path, PAUSE_NAME);
    FILE *file = fopen(pause_path, "w");
    if (!file) {
        set_api_error(error, error_size, "could not create pause request");
        return 0;
    }
    fclose(file);
    return 1;
}
