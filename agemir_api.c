/*
 * The legacy core currently lives in Agemir.c and its included C modules.
 * Compile this translation unit for consumers of the C API; the core is
 * compiled exactly once and the interactive CLI main is omitted.
 */
#define AGEMIR_NO_MAIN
#include "Agemir.c"

#include <stdatomic.h>

struct AgemirRuntime {
    int benchmark_tps;
    atomic_flag status_lock;
    AgemirStatus status;
    char active_project_path[512];
    int status_active;
};

static void lock_status(AgemirRuntime *runtime) {
    while (atomic_flag_test_and_set_explicit(&runtime->status_lock, memory_order_acquire)) {}
}

static void unlock_status(AgemirRuntime *runtime) {
    atomic_flag_clear_explicit(&runtime->status_lock, memory_order_release);
}

static void copy_status_text(char *destination, size_t size, const char *source) {
    if (!destination || size == 0) return;
    snprintf(destination, size, "%s", source ? source : "");
}

static void api_progress_listener(const AgentProgress *progress, void *context) {
    AgemirRuntime *runtime = (AgemirRuntime *)context;
    if (!runtime || !progress) return;
    lock_status(runtime);
    copy_status_text(runtime->status.phase, sizeof(runtime->status.phase), progress->phase);
    const char *legacy_state = strcmp(progress->phase, "completed") == 0 ? "finished" :
        strcmp(progress->phase, "paused") == 0 ? "paused" :
        strcmp(progress->phase, "failed") == 0 ? "failed" : "running";
    copy_status_text(runtime->status.state, sizeof(runtime->status.state), legacy_state);
    copy_status_text(runtime->status.task, sizeof(runtime->status.task), progress->task);
    copy_status_text(runtime->status.current_task, sizeof(runtime->status.current_task), progress->task);
    copy_status_text(runtime->status.current_file, sizeof(runtime->status.current_file), progress->file);
    copy_status_text(runtime->status.current_command, sizeof(runtime->status.current_command), progress->command);
    copy_status_text(runtime->status.reason, sizeof(runtime->status.reason), progress->reason);
    if (progress->total_tasks > 0 || runtime->status.total_tasks == 0)
        runtime->status.total_tasks = progress->total_tasks;
    if (progress->completed_tasks > 0 || progress->complete || progress->failed || progress->paused)
        runtime->status.completed_tasks = progress->completed_tasks;
    runtime->status.attempts = progress->retry_attempt;
    runtime->status.retry_attempt = progress->retry_attempt;
    runtime->status.pause_requested = progress->pause_requested;
    runtime->status.paused = progress->paused;
    runtime->status.complete = progress->complete;
    runtime->status.failed = progress->failed;
    runtime->status_active = 1;
    unlock_status(runtime);
}

static void api_set_phase(AgemirRuntime *runtime, const char *path, const char *phase,
                          const char *reason, int failed, int complete) {
    if (!runtime) return;
    lock_status(runtime);
    memset(&runtime->status, 0, sizeof(runtime->status));
    copy_status_text(runtime->active_project_path, sizeof(runtime->active_project_path), path);
    copy_status_text(runtime->status.phase, sizeof(runtime->status.phase), phase);
    copy_status_text(runtime->status.state, sizeof(runtime->status.state), phase);
    copy_status_text(runtime->status.reason, sizeof(runtime->status.reason), reason);
    runtime->status.failed = failed;
    runtime->status.complete = complete;
    runtime->status_active = 1;
    unlock_status(runtime);
}

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

/* API callers cannot answer the legacy CLI's install prompt on stdin. */
static int prepare_api_execution(char *error, size_t error_size) {
#if OLLAMA_ENABLED
    if (!ollama_installed()) {
        set_api_error(error, error_size,
                      "Ollama is not installed; install it before starting this request.");
        return 0;
    }
#endif
    if (!ensure_ollama_ready()) {
        set_api_error(error, error_size,
                      "Ollama could not be started; check that it is installed and available.");
        return 0;
    }
    return 1;
}

AgemirRuntime *agemir_runtime_create(void) {
    AgemirRuntime *runtime = (AgemirRuntime *)calloc(1, sizeof(*runtime));
    if (!runtime) return NULL;
    atomic_flag_clear(&runtime->status_lock);

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

int agemir_runtime_ollama_available(void) {
    return ollama_installed();
}

int agemir_runtime_get_setting(AgemirRuntime *runtime, const char *key,
                               char *value, size_t value_size) {
    if (!runtime || !key || !value || value_size == 0) return 0;
    char setting[512];
    if (strcmp(key, "username") == 0) {
        if (!config_get(key, setting, sizeof(setting))) setting[0] = '\0';
        return copy_api_text(value, value_size, setting);
    }
    if (strcmp(key, "model") == 0) {
        if (!config_get(key, setting, sizeof(setting)))
            snprintf(setting, sizeof(setting), "auto");
        return copy_api_text(value, value_size, setting);
    }
    if (strcmp(key, "project_path") == 0) {
        if (!config_get(key, setting, sizeof(setting))) setting[0] = '\0';
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
    if (strcmp(key, "project_path") == 0) {
        if (strlen(value) >= 512 || strpbrk(value, "\r\n")) return 0;
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

int agemir_runtime_model_count(AgemirRuntime *runtime) {
    return runtime ? catalog_count : -1;
}

int agemir_runtime_model_at(AgemirRuntime *runtime, int index, AgemirModel *model) {
    if (!runtime || !model || index < 0 || index >= catalog_count) return 0;
    const ModelInfo *source = &catalog[index];
    memset(model, 0, sizeof(*model));
    snprintf(model->tag, sizeof(model->tag), "%s", source->tag);
    model->min_ram_gb = source->min_ram_gb;
    model->fits = model_fits(source, total_ram_mb());
    model->estimated_tokens_per_second = estimated_speed(runtime->benchmark_tps, source);
    snprintf(model->note, sizeof(model->note), "%s", source->note);
    return 1;
}

int agemir_runtime_current_model(AgemirRuntime *runtime, char *tag, size_t tag_size,
                                 int *is_automatic) {
    if (!runtime || !tag || !tag_size || !is_automatic) return 0;
    char selected[64];
    current_model(runtime->benchmark_tps, total_ram_mb(), selected, sizeof(selected), is_automatic);
    return copy_api_text(tag, tag_size, selected);
}

int agemir_runtime_execute(AgemirRuntime *runtime, const AgemirRequest *request,
                           char *error, size_t error_size) {
    set_api_error(error, error_size, "");
    if (!runtime || !request) {
        set_api_error(error, error_size, "runtime and request are required");
        return 0;
    }

    api_set_phase(runtime, request->project_path, "preparing", "", 0, 0);

    char selected_model[64];
    const char *model = request->model;
    int is_auto = !model || !*model || strcmp(model, "auto") == 0;
    if (is_auto) {
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
        char ledger_path[1024];
        join_path(ledger_path, sizeof(ledger_path), request->project_path, "tasks.txt");
        if (!file_exists(ledger_path)) {
            set_api_error(error, error_size, "tasks.txt not found in the project path");
            return 0;
        }
        if (!prepare_api_execution(error, error_size)) {
            api_set_phase(runtime, request->project_path, "failed", error, 1, 0);
            return 0;
        }
        agent_progress_listener = api_progress_listener;
        agent_progress_context = runtime;
        if (!run_agent(request->project_path, model)) {
            agent_progress_listener = NULL;
            agent_progress_context = NULL;
            set_api_error(error, error_size, "project run could not start; see console output");
            api_set_phase(runtime, request->project_path, "failed", error, 1, 0);
            return 0;
        }
        agent_progress_listener = NULL;
        agent_progress_context = NULL;
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
        /* Keep the CLI's generation-specific automatic choice. */
        if (is_auto && total_ram_mb() >= 12 * 1024 && runtime->benchmark_tps >= 10)
            model = "qwen2.5-coder:7b";
        if (!prepare_api_execution(error, error_size)) {
            api_set_phase(runtime, request->project_path, "failed", error, 1, 0);
            return 0;
        }
        if (!generate_project(model, request->project_name, request->project_path,
                              request->description, request->graphical != 0)) {
            set_api_error(error, error_size, "project generation failed; see console output");
            api_set_phase(runtime, request->project_path, "failed", error, 1, 0);
            return 0;
        }
        api_set_phase(runtime, request->project_path, "completed", "", 0, 1);
        return 1;
    }
    if (request->operation == AGEMIR_GENERATE_TEMPLATE) {
        if (file_exists(TEMPLATE_OUTPUT)) {
            set_api_error(error, error_size, "project_template.txt already exists");
            api_set_phase(runtime, NULL, "completed", error, 0, 1);
            return 1;
        }
        if (!copy_file(TEMPLATE_SOURCE, TEMPLATE_OUTPUT)) {
            set_api_error(error, error_size, "could not read the bundled project template");
            return 0;
        }
        set_api_error(error, error_size, "Created project_template.txt. Fill it in, then import it.");
        api_set_phase(runtime, NULL, "completed", error, 0, 1);
        return 1;
    }
    if (request->operation == AGEMIR_IMPORT_TEMPLATE) {
        if (!request->description || !*request->description || !request->project_path || !*request->project_path) {
            set_api_error(error, error_size, "template file and project folder are required");
            return 0;
        }
        if (!import_project(request->description, request->project_path)) {
            set_api_error(error, error_size, "template import failed; see console output");
            api_set_phase(runtime, request->project_path, "failed", error, 1, 0);
            return 0;
        }
        api_set_phase(runtime, request->project_path, "completed", "", 0, 1);
        return 1;
    }
    if (request->operation == AGEMIR_RUN_BENCHMARK) {
        int rate = run_hardware_benchmark();
        if (rate < 1) {
            set_api_error(error, error_size, "hardware benchmark failed; see console output");
            api_set_phase(runtime, NULL, "failed", error, 1, 0);
            return 0;
        }
        runtime->benchmark_tps = rate;
        snprintf(error, error_size, "Hardware benchmark completed: %d output tokens/sec.", rate);
        api_set_phase(runtime, NULL, "completed", error, 0, 1);
        return 1;
    }
    set_api_error(error, error_size, "unsupported operation");
    return 0;
}

int agemir_runtime_get_status(AgemirRuntime *runtime, const char *project_path,
                              AgemirStatus *status) {
    if (!runtime || !status) return 0;
    lock_status(runtime);
    if (runtime->status_active && (!project_path || !*project_path ||
        strcmp(runtime->active_project_path, project_path) == 0)) {
        memcpy(status, &runtime->status, sizeof(*status));
        unlock_status(runtime);
        return 1;
    }
    unlock_status(runtime);
    if (!project_path || !*project_path) return 0;
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
    copy_status_text(status->phase, sizeof(status->phase), status->state);
    copy_status_text(status->current_task, sizeof(status->current_task), status->task);
    status->retry_attempt = status->attempts;
    status->paused = strcmp(status->state, "paused") == 0;
    status->complete = strcmp(status->state, "finished") == 0;
    status->failed = strcmp(status->state, "failed") == 0 || strcmp(status->state, "error") == 0;
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
    lock_status(runtime);
    if (runtime->status_active && strcmp(runtime->active_project_path, project_path) == 0)
        runtime->status.pause_requested = 1;
    unlock_status(runtime);
    return 1;
}
