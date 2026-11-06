#ifndef AGENT_H
#define AGENT_H

#include "project.h"

#define MAX_ATTEMPTS 5
#define MAX_INCLUDED_BYTES 4000
#define PAYLOAD_NAME ".agemir_payload.txt"
#define STDERR_NAME ".agemir_stderr.txt"
#define VERIFY_OUTPUT_NAME ".agemir_verify.txt"
#define STATE_NAME "progress_state.txt"
#define PAUSE_NAME "PAUSE"
#define LOG_DIR_NAME "logs"
#define GENERATOR_TARGET "__template__"

typedef enum {
    TASK_DONE,
    TASK_FAILED,
    TASK_PAUSED,
    TASK_ERROR
} TaskResult;

// Set by Ctrl+C while the agent runs. The agent finishes the current step and pauses.
static volatile sig_atomic_t pause_requested = 0;

void handle_interrupt(int signal_number) {
    (void)signal_number;
    pause_requested = 1;
}

// ==========================================
//      Ollama Control
// ==========================================
int ollama_installed() {
#if defined(_WIN32)
    // WinGet updates the user PATH, but the running Agemir process still has its old PATH.
    if (system("where.exe ollama.exe" DEV_NULL) == 0) return 1;
    const char* local = getenv("LOCALAPPDATA");
    if (!local) return 0;

    char directory[1024], executable[1100];
    int length = snprintf(directory, sizeof(directory), "%s\\Programs\\Ollama", local);
    if (length < 0 || (size_t)length >= sizeof(directory)) return 0;
    snprintf(executable, sizeof(executable), "%s\\ollama.exe", directory);
    if (!file_exists(executable)) return 0;

    const char* old_path = getenv("PATH");
    if (!old_path) old_path = "";
    size_t size = strlen(directory) + strlen(old_path) + 2;
    char* updated_path = malloc(size);
    if (!updated_path) return 0;
    snprintf(updated_path, size, "%s;%s", directory, old_path);
    int ok = _putenv_s("PATH", updated_path) == 0;
    free(updated_path);
    return ok;
#else
    return system("command -v ollama" DEV_NULL) == 0;
#endif
}

int ollama_server_ready() {
    return system("ollama list" DEV_NULL) == 0;
}

// Installs Ollama (after asking) and starts its server when needed.
int ensure_ollama_ready() {
#if OLLAMA_ENABLED
    if (!ollama_installed()) {
        if (!ask_yes_no(ui_strings[47])) {
            printf("%s %s\n", ui_strings[33], INSTALL_OLLAMA);
            return 0;
        }
        printf("%s\n", ui_strings[48]);
        system(INSTALL_OLLAMA);
        if (!ollama_installed()) {
            printf("%s %s\n", ui_strings[33], INSTALL_OLLAMA);
            return 0;
        }
    }

    if (!ollama_server_ready()) {
        printf("%s\n", ui_strings[49]);
        int status = system(START_OLLAMA_SERVER);
        (void)status;
        for (int i = 0; i < 20 && !ollama_server_ready(); i++) {
            sleep_seconds(1);
        }
        if (!ollama_server_ready()) {
            printf("%s\n", ui_strings[50]);
            return 0;
        }
    }
#endif
    return 1;
}

// Downloads the model when it is missing. The download progress is shown on screen.
int ensure_model(const char* model) {
#if OLLAMA_ENABLED
    char command[512];
    snprintf(command, sizeof(command), "ollama pull %s", model);
    if (system(command) != 0) {
        printf("%s could not download %s\n", ui_strings[55], model);
        return 0;
    }
#else
    (void)model;
#endif
    return 1;
}

// Frees the RAM held by the model so the next step starts from a clean machine.
void unload_model(const char* model) {
#if OLLAMA_ENABLED
    char command[512];
    snprintf(command, sizeof(command), "ollama stop %s %s", model, DEV_NULL);
    int status = system(command);
    (void)status;
#else
    (void)model;
#endif
}

// Returns what the model printed on stdout. Anything it printed on stderr goes to stderr_path.
char* run_model(const char* model, const char* payload_path, const char* stderr_path, const char* target_file) {
#if OLLAMA_ENABLED
    (void)target_file;

    char command[4608];
    snprintf(command, sizeof(command), "ollama run %s --nowordwrap < \"%s\" 2> \"%s\"",
             model, payload_path, stderr_path);
    FILE *pipe = POPEN(command, "r");
    if (!pipe) return NULL;

    size_t length = 0;
    size_t capacity = 4096;
    char* output = malloc(capacity);
    char chunk[1024];
    size_t received;

    while (output && (received = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        while (length + received + 1 > capacity) capacity *= 2;
        char* grown = realloc(output, capacity);
        if (!grown) {
            free(output);
            output = NULL;
            break;
        }
        output = grown;
        memcpy(output + length, chunk, received);
        length += received;
    }
    if (output) output[length] = '\0';

    int failed = ferror(pipe);
    if (PCLOSE(pipe) != 0 || failed) {
        free(output);
        return NULL;
    }
    return output;
#else
    (void)model;
    (void)payload_path;
    remove(stderr_path);

    if (strcmp(target_file, GENERATOR_TARGET) == 0) {
        char* example = read_file(EXAMPLE_TEMPLATE_PATH);
        size_t size = (example ? strlen(example) : 0) + 64;
        char* output = malloc(size);
        if (output) snprintf(output, size, "```\n%s\n```\n", example ? example : "");
        free(example);
        return output;
    }

    char* output = malloc(strlen(target_file) + 64);
    if (output) snprintf(output, strlen(target_file) + 64, "```\nSimulated output for %s\n```\n", target_file);
    return output;
#endif
}

// The first "Error: ..." line Ollama printed, if any. Retrying cannot fix those.
int first_error_line(const char* errors, char* line, size_t size) {
    if (!errors) return 0;

    const char* match = strstr(errors, "Error:");
    if (!match) match = strstr(errors, "error:");
    if (!match) return 0;

    size_t length = strcspn(match, "\r\n");
    if (length >= size) length = size - 1;
    memcpy(line, match, length);
    line[length] = '\0';
    return 1;
}

// ==========================================
//      Payload
// ==========================================
char* read_file_capped(const char* path, size_t max_bytes) {
    char* text = read_file(path);
    if (!text || strlen(text) <= max_bytes) return text;

    char* capped = malloc(max_bytes + 32);
    if (capped) {
        memcpy(capped, text, max_bytes);
        strcpy(capped + max_bytes, "\n[...truncated]\n");
    }
    free(text);
    return capped;
}

void write_reads(FILE* payload, const char* project_path, const char* reads) {
    char list[512];
    snprintf(list, sizeof(list), "%s", reads);

    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        name = trim(name);
        char path[1024];
        join_path(path, sizeof(path), project_path, name);

        char* text = read_file_capped(path, MAX_INCLUDED_BYTES);
        if (text) fprintf(payload, "### FILE %s ###\n%s\n\n", name, text);
        free(text);
    }
}

// UI templates and icon/image links, only for tasks that ask for them with UI.
void write_ui_templates(FILE* payload, const char* project_path, const char* ui) {
    if (ui[0] == '\0') return;

    char list[256];
    snprintf(list, sizeof(list), "%s", ui);
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        name = trim(name);
        char relative[512], path[1024];
        snprintf(relative, sizeof(relative), "templates/ui/%s", name);
        join_path(path, sizeof(path), project_path, relative);

        char* text = read_file_capped(path, MAX_INCLUDED_BYTES);
        if (text) fprintf(payload, "### UI TEMPLATE %s (follow this design) ###\n%s\n\n", name, text);
        free(text);
    }

    char links_path[1024];
    join_path(links_path, sizeof(links_path), project_path, "templates/asset_links.txt");
    char* links = read_file_capped(links_path, 1500);
    if (links) fprintf(payload, "### ASSET LINKS (use only these for icons and images) ###\n%s\n\n", links);
    free(links);
}

int write_payload(const char* project_path, const Task* task, const char* feedback) {
    char payload_path[1024], context_path[1024], style_path[1024], target_path[1024], ledger_path[1024];
    join_path(payload_path, sizeof(payload_path), project_path, PAYLOAD_NAME);
    join_path(context_path, sizeof(context_path), project_path, "context.md");
    join_path(style_path, sizeof(style_path), project_path, "templates/ui_guidelines.md");
    join_path(target_path, sizeof(target_path), project_path, task->file);
    join_path(ledger_path, sizeof(ledger_path), project_path, "tasks.txt");

    FILE *payload = fopen(payload_path, "w");
    if (!payload) return 0;

    char* context_text = read_file_capped(context_path, MAX_INCLUDED_BYTES);
    char* style_text = read_file_capped(style_path, MAX_INCLUDED_BYTES);
    char* current_text = read_file_capped(target_path, MAX_INCLUDED_BYTES);

    fprintf(payload,
            "SYSTEM: You are a coding agent completing one task of a larger project.\n"
            "Reply with ONLY the complete final content of the file `%s` inside a single fenced code block.\n"
            "Do not explain anything and do not write any other file.\n\n",
            task->file);
    fprintf(payload, "### PROJECT CONTEXT ###\n%s\n\n", context_text ? context_text : "");
    fprintf(payload, "### STYLE GUIDELINES ###\n%s\n\n", style_text ? style_text : "");
    fprintf(payload, "### VERIFIED FILES ###\n");
    ledger_write_built_summary(payload, ledger_path, task->reads);
    fprintf(payload, "\n### CURRENT GOAL ###\n%s\n\n", task->goal);
    write_reads(payload, project_path, task->reads);
    write_ui_templates(payload, project_path, task->ui);
    fprintf(payload, "### CURRENT CONTENT OF %s ###\n%s\n\n", task->file, current_text ? current_text : "(new file)");
    fprintf(payload, "### TASK ###\n%s\n", task->description);
    if (task->verify[0]) fprintf(payload, "Acceptance check: %s\n", task->verify);
    if (feedback[0] != '\0') {
        fprintf(payload, "\n### PREVIOUS ATTEMPT FAILED ###\n%s\n", feedback);
    }
    fprintf(payload, "\nREMINDER: reply with ONLY the complete content of `%s` in one fenced code block.\n", task->file);

    free(context_text);
    free(style_text);
    free(current_text);
    fclose(payload);
    return 1;
}

// ==========================================
//      Applying & Verifying Output
// ==========================================
// Returns the content of the outermost fenced code block, or NULL if the
// reply has no complete, non-empty block.
char* extract_code_block(const char* text) {
    const char* open_fence = strstr(text, "```");
    if (!open_fence) return NULL;

    const char* body = strchr(open_fence, '\n');
    if (!body) return NULL;
    body++;

    const char* close_fence = NULL;
    for (const char* p = strstr(body, "\n```"); p; p = strstr(p + 1, "\n```")) {
        close_fence = p;
    }
    if (!close_fence) return NULL;

    size_t length = (size_t)(close_fence - body) + 1;
    char* code = malloc(length + 1);
    if (!code) return NULL;
    memcpy(code, body, length);
    code[length] = '\0';

    const char* check = code;
    while (isspace((unsigned char)*check)) check++;
    if (*check == '\0') {
        free(code);
        return NULL;
    }
    return code;
}

// True when the user pressed Ctrl+C while a shell command was running.
int was_interrupted(int status) {
#if defined(_WIN32)
    (void)status;
    return 0;
#else
    return (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) ||
           (WIFEXITED(status) && WEXITSTATUS(status) == 130);
#endif
}

// Verification only runs against real model output; simulated output is placeholder text.
int verify_task(const char* project_path, const Task* task, char* feedback, size_t feedback_size, int* interrupted) {
    *interrupted = 0;
    if (task->verify[0] == '\0' || !OLLAMA_ENABLED) return 1;

    char command[2048];
    snprintf(command, sizeof(command), VERIFY_COMMAND_FORMAT, project_path, task->verify, VERIFY_OUTPUT_NAME);
    int status = system(command);

    char output_path[1024];
    join_path(output_path, sizeof(output_path), project_path, VERIFY_OUTPUT_NAME);

    if (status != 0) {
        *interrupted = was_interrupted(status);

        char* output = read_file(output_path);
        const char* tail = output ? output : "";
        size_t length = strlen(tail);
        if (length > 1500) tail += length - 1500;
        snprintf(feedback, feedback_size, "The verification command `%s` failed with this output:\n%s",
                 task->verify, tail);
        free(output);
    }
    remove(output_path);
    return status == 0;
}

// ==========================================
//      Logs & Progress State
// ==========================================
void format_time(char* out, size_t size) {
    time_t now = time(NULL);
    strftime(out, size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

void sanitize_name(const char* source, char* out, size_t size) {
    size_t i = 0;
    for (; source[i] && i + 1 < size; i++) {
        unsigned char c = (unsigned char)source[i];
        out[i] = (isalnum(c) || c == '.' || c == '-' || c == '_') ? (char)c : '_';
    }
    out[i] = '\0';
}

// One file per task and attempt in <project>/logs/, so a failure can be debugged afterwards.
void write_attempt_log(const char* project_path, const Task* task, int attempt, const char* model,
                       const char* feedback, const char* output, const char* errors, const char* result) {
    char directory[1024], base[64], name[128], path[1300], stamp[32];
    join_path(directory, sizeof(directory), project_path, LOG_DIR_NAME);
    MKDIR(directory);

    sanitize_name(task_label(task), base, sizeof(base));
    snprintf(name, sizeof(name), "%s.attempt%d.log", base, attempt);
    join_path(path, sizeof(path), directory, name);

    FILE *f = fopen(path, "w");
    if (!f) return;

    format_time(stamp, sizeof(stamp));
    fprintf(f, "time: %s\nmodel: %s\ntask: %s\nattempt: %d/%d\nresult: %s\n\n",
            stamp, model, task_label(task), attempt, MAX_ATTEMPTS, result);
    fprintf(f, "--- feedback given to the model ---\n%s\n\n", feedback);
    fprintf(f, "--- model output ---\n%s\n\n", output ? output : "(none)");
    fprintf(f, "--- ollama stderr ---\n%s\n", errors ? errors : "(none)");
    fclose(f);
}

// Where the run stopped, so the next run can pick up exactly there.
void save_state(const char* project_path, const char* status, const char* task, int attempts, const char* reason) {
    char path[1024], number[16], stamp[32], short_reason[200];
    join_path(path, sizeof(path), project_path, STATE_NAME);

    snprintf(number, sizeof(number), "%d", attempts);
    format_time(stamp, sizeof(stamp));
    snprintf(short_reason, sizeof(short_reason), "%.*s", (int)strcspn(reason, "\r\n"), reason);

    kv_set(path, "status", status);
    kv_set(path, "task", task);
    kv_set(path, "attempts", number);
    kv_set(path, "reason", short_reason);
    kv_set(path, "updated", stamp);
}

void log_progress(const char* project_path, const Task* task, int attempts, TaskResult result) {
    FILE *f = open_project_file(project_path, "progress_log.md", "a");
    if (!f) return;

    char stamp[32];
    format_time(stamp, sizeof(stamp));

    if (result == TASK_DONE) {
        fprintf(f, "- %s [done] %s (attempts: %d): %s\n", stamp, task_label(task), attempts, task->description);
    } else if (result == TASK_FAILED) {
        fprintf(f, "- %s [FAILED] %s after %d attempts, see %s/: %s\n",
                stamp, task_label(task), attempts, LOG_DIR_NAME, task->description);
    } else if (result == TASK_PAUSED) {
        fprintf(f, "- %s [paused] before finishing %s\n", stamp, task_label(task));
    } else {
        fprintf(f, "- %s [ollama error] on %s, see %s/\n", stamp, task_label(task), LOG_DIR_NAME);
    }
    fclose(f);
}

int pause_pending(const char* project_path) {
    char path[1024];
    join_path(path, sizeof(path), project_path, PAUSE_NAME);
    return pause_requested || file_exists(path);
}

// ==========================================
//      Task Execution
// ==========================================
// A command task (RUN) runs a shell command inside the project folder, no model involved.
TaskResult execute_command_task(const char* project_path, const Task* task, int* attempts_used) {
    char command[2048], output_path[1024];
    snprintf(command, sizeof(command), VERIFY_COMMAND_FORMAT, project_path, task->verify, VERIFY_OUTPUT_NAME);
    join_path(output_path, sizeof(output_path), project_path, VERIFY_OUTPUT_NAME);

    TaskResult result = TASK_FAILED;
    *attempts_used = 0;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS && result == TASK_FAILED; attempt++) {
        if (pause_pending(project_path)) {
            result = TASK_PAUSED;
            break;
        }

        *attempts_used = attempt;
        printf("%s %d/%d\n", ui_strings[57], attempt, MAX_ATTEMPTS);
        save_state(project_path, "running", task_label(task), attempt, "");

        int status = system(command);
        if (was_interrupted(status) || pause_requested) {
            result = TASK_PAUSED;
            break;
        }

        char* output = read_file(output_path);
        remove(output_path);
        write_attempt_log(project_path, task, attempt, "(shell command)", "", output, "",
                          status == 0 ? "ok" : "the command failed");
        free(output);

        if (status == 0) {
            result = TASK_DONE;
        } else if (attempt < MAX_ATTEMPTS) {
            sleep_seconds(1);
        }
    }
    return result;
}

// Writes one file with the model. Retries with the error output as feedback.
// A task that does not succeed leaves the target file exactly as it was.
TaskResult execute_file_task(const char* project_path, const char* model, const Task* task,
                             int* attempts_used, char* error_line, size_t error_size) {
    char target_path[1024], payload_path[1024], stderr_path[1024];
    join_path(target_path, sizeof(target_path), project_path, task->file);
    join_path(payload_path, sizeof(payload_path), project_path, PAYLOAD_NAME);
    join_path(stderr_path, sizeof(stderr_path), project_path, STDERR_NAME);

    char* original = read_file(target_path);
    char feedback[2048] = "";
    TaskResult result = TASK_FAILED;
    *attempts_used = 0;
    error_line[0] = '\0';

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (pause_pending(project_path)) {
            result = TASK_PAUSED;
            break;
        }

        *attempts_used = attempt;
        printf("%s %d/%d\n", ui_strings[57], attempt, MAX_ATTEMPTS);
        save_state(project_path, "running", task->file, attempt, "");

        if (!write_payload(project_path, task, feedback)) {
            snprintf(error_line, error_size, "cannot write %s", payload_path);
            result = TASK_ERROR;
            break;
        }

        printf("%s\n", ui_strings[13]);
        char* output = run_model(model, payload_path, stderr_path, task->file);
        unload_model(model);
        printf("%s\n", ui_strings[14]);

        char* errors = read_file(stderr_path);
        char* code = NULL;
        int stop = 0;

        if (pause_requested) {
            result = TASK_PAUSED;
            stop = 1;
        } else if ((code = output ? extract_code_block(output) : NULL) == NULL) {
            if (first_error_line(errors, error_line, error_size) || !output) {
                if (error_line[0] == '\0') snprintf(error_line, error_size, "could not run ollama");
                write_attempt_log(project_path, task, attempt, model, feedback, output, errors, error_line);
                result = TASK_ERROR;
                stop = 1;
            } else {
                snprintf(feedback, sizeof(feedback),
                         "Your reply did not contain a complete fenced code block with the file content.");
                write_attempt_log(project_path, task, attempt, model, feedback, output, errors,
                                  "no complete fenced code block in the reply");
            }
        } else {
            int interrupted = 0;
            write_text_file(target_path, code);
            if (verify_task(project_path, task, feedback, sizeof(feedback), &interrupted)) {
                write_attempt_log(project_path, task, attempt, model, feedback, output, errors, "ok");
                result = TASK_DONE;
                stop = 1;
            } else if (interrupted) {
                result = TASK_PAUSED;
                stop = 1;
            } else {
                write_attempt_log(project_path, task, attempt, model, "", output, errors, feedback);
            }
        }

        free(code);
        free(output);
        free(errors);
        if (stop) break;
    }

    if (result != TASK_DONE) {
        if (original) write_text_file(target_path, original);
        else remove(target_path);
    }

    free(original);
    remove(payload_path);
    remove(stderr_path);
    return result;
}

// ==========================================
//      Orchestrator Loop
// ==========================================
// Runs every pending task in order. It stops at the first task that keeps failing
// (nothing after it can be trusted), keeps the progress, and resumes from there next time.
int run_agent(const char* project_path, const char* model) {
    char ledger_path[1024], state_path[1024], pause_path[1024], logs_path[1024];
    join_path(ledger_path, sizeof(ledger_path), project_path, "tasks.txt");
    join_path(state_path, sizeof(state_path), project_path, STATE_NAME);
    join_path(pause_path, sizeof(pause_path), project_path, PAUSE_NAME);
    join_path(logs_path, sizeof(logs_path), project_path, LOG_DIR_NAME);

    if (!file_exists(ledger_path)) {
        printf("%s\n", ui_strings[11]);
        return 0;
    }
    if (!ensure_ollama_ready() || !ensure_model(model)) {
        return 0;
    }

    remove(pause_path);
    pause_requested = 0;
    signal(SIGINT, handle_interrupt);

    char previous_status[32] = "";
    char previous_task[512] = "";
    kv_get(state_path, "status", previous_status, sizeof(previous_status));
    kv_get(state_path, "task", previous_task, sizeof(previous_task));
    if (strcmp(previous_status, "finished") != 0 && previous_task[0] != '\0') {
        printf("%s %s\n", ui_strings[54], previous_task);
    }

    printf("%s\n", ui_strings[32]);
    ledger_reset_failed(ledger_path);

    int completed = 0;
    int stopped = 0;
    Task task;
    memset(&task, 0, sizeof(task));

    while (!stopped && ledger_next_pending(ledger_path, &task)) {
        printf("\n%s %s\n", ui_strings[12], task.description);

        int attempts = 0;
        char error_line[256] = "";
        TaskResult result;
        if (task.is_command) {
            result = execute_command_task(project_path, &task, &attempts);
        } else {
            result = execute_file_task(project_path, model, &task, &attempts, error_line, sizeof(error_line));
        }
        log_progress(project_path, &task, attempts, result);

        if (result == TASK_DONE) {
            ledger_mark(ledger_path, task.ledger_offset, 'x');
            completed++;
            printf("%s %s\n", ui_strings[29], task_label(&task));
        } else if (result == TASK_FAILED) {
            ledger_mark(ledger_path, task.ledger_offset, '!');
            save_state(project_path, "failed", task_label(&task), attempts, "the task kept failing");
            printf("\n%s %s\n%s\n%s %s\n", ui_strings[30], task_label(&task), ui_strings[51], ui_strings[52], logs_path);
            stopped = 1;
        } else if (result == TASK_PAUSED) {
            save_state(project_path, "paused", task_label(&task), attempts, "paused by the user");
            printf("\n%s\n", ui_strings[53]);
            stopped = 1;
        } else {
            save_state(project_path, "error", task_label(&task), attempts, error_line);
            printf("\n%s %s\n%s\n%s %s\n", ui_strings[55], error_line, ui_strings[56], ui_strings[52], logs_path);
            stopped = 1;
        }
    }

    if (!stopped) {
        save_state(project_path, "finished", "", 0, "");
        printf("\n%s (ok: %d)\n", ui_strings[31], completed);
    }

    signal(SIGINT, SIG_DFL);
    return 1;
}

#endif