#ifndef AGENT_H
#define AGENT_H

#include "project.h"

#define MAX_ATTEMPTS 5
#define MAX_INCLUDED_BYTES 4000
#define MAX_SOURCES_BYTES 1500
#define PAYLOAD_NAME ".agemir_payload.txt"
#define VERIFY_OUTPUT_NAME ".agemir_verify.txt"
#define PAUSE_FILE_NAME "PAUSE"
#define ATTENTION_FILE_NAME "NEEDS_ATTENTION.md"

typedef enum { RUN_FINISHED, RUN_PAUSED, RUN_STOPPED, RUN_ERROR } RunResult;

void request_pause(int signal_number) {
    (void)signal_number;
    pause_requested = 1;
}

// ==========================================
//      Ollama Control
// ==========================================
int ollama_installed() {
    return system("ollama --version" DEV_NULL) == 0;
}

int ollama_server_ready() {
    return system("ollama list" DEV_NULL) == 0;
}

// Installs Ollama (after asking) and starts its server when needed.
// Does nothing in simulation mode.
int ensure_ollama() {
#if OLLAMA_ENABLED
    if (!ollama_installed()) {
        char answer[16];
        printf("%s ", ui_strings[53]);
        read_line(answer, sizeof(answer));
        if (tolower((unsigned char)answer[0]) != 'y') {
            printf("%s %s\n", ui_strings[33], INSTALL_OLLAMA);
            return 0;
        }

        printf("%s\n", ui_strings[54]);
        int status = system(INSTALL_OLLAMA);
        if (status != 0 || !ollama_installed()) {
            printf("%s %s\n", ui_strings[33], INSTALL_OLLAMA);
            return 0;
        }
    }

    if (!ollama_server_ready()) {
        printf("%s\n", ui_strings[55]);
        int status = system(START_OLLAMA_SERVER);
        (void)status;
        for (int i = 0; i < 20 && !ollama_server_ready(); i++) {
            SLEEP_SECONDS(1);
        }
        if (!ollama_server_ready()) {
            printf("%s\n", ui_strings[56]);
            return 0;
        }
    }
#endif
    return 1;
}

void ensure_model(const char* model) {
#if OLLAMA_ENABLED
    char command[512];
    snprintf(command, sizeof(command), "ollama pull %s %s", model, DEV_NULL);
    run_shell(command, 0);
#else
    (void)model;
#endif
}

// Frees the RAM held by the model so the next task starts from a clean machine.
void unload_model(const char* model) {
#if OLLAMA_ENABLED
    char command[512];
    snprintf(command, sizeof(command), "ollama stop %s %s", model, DEV_NULL);
    run_shell(command, 30);
#else
    (void)model;
#endif
}

#if !OLLAMA_ENABLED
// Placeholder reply used in simulation mode. Template requests get a minimal valid template.
char* simulated_reply(const char* target_file) {
    const char* suffix = "_gen_template.txt";
    size_t file_length = strlen(target_file);
    size_t suffix_length = strlen(suffix);

    char* reply = malloc(file_length + 512);
    if (!reply) return NULL;

    if (file_length > suffix_length && strcmp(target_file + file_length - suffix_length, suffix) == 0) {
        snprintf(reply, file_length + 512,
                 "```\nPROJECT_NAME: %.*s\nOBJECTIVE: Simulated objective.\nTECH_STACK: Simulated stack.\n\n"
                 "GOAL: Simulated goal\nTASK: Simulated task\nFILE: src/simulated.txt\n```\n",
                 (int)(file_length - suffix_length), target_file);
    } else {
        snprintf(reply, file_length + 512, "```\nSimulated output for %s\n```\n", target_file);
    }
    return reply;
}
#endif

char* run_model(const char* model, const char* payload_path, const char* target_file) {
#if OLLAMA_ENABLED
    (void)target_file;

    char command[2048];
    snprintf(command, sizeof(command), "ollama run %s --nowordwrap < \"%s\" 2>&1", model, payload_path);
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

    PCLOSE(pipe);
    return output;
#else
    (void)model;
    (void)payload_path;
    return simulated_reply(target_file);
#endif
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
        snprintf(path, sizeof(path), "%s/%s", project_path, name);

        char* text = read_file_capped(path, MAX_INCLUDED_BYTES);
        if (text) fprintf(payload, "### FILE %s ###\n%s\n\n", name, text);
        free(text);
    }
}

// UI templates the task asked for, plus the list of free icon and image links.
void write_ui_resources(FILE* payload, const char* project_path, const char* ui) {
    if (ui[0] == '\0') return;

    char list[256];
    snprintf(list, sizeof(list), "%s", ui);
    fprintf(payload, "### UI TEMPLATES (follow this look and structure) ###\n");
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        name = trim(name);
        char path[1024];
        snprintf(path, sizeof(path), "%s/templates/ui/%s", project_path, name);

        char* text = read_file_capped(path, MAX_INCLUDED_BYTES);
        if (text) fprintf(payload, "--- %s ---\n%s\n", name, text);
        free(text);
    }

    char sources_path[1024];
    snprintf(sources_path, sizeof(sources_path), "%s/templates/internet_sources.txt", project_path);
    char* sources = read_file_capped(sources_path, MAX_SOURCES_BYTES);
    if (sources) fprintf(payload, "\n### ICONS AND IMAGES YOU MAY USE (direct links) ###\n%s\n", sources);
    free(sources);
    fprintf(payload, "\n");
}

int write_payload(const char* project_path, const Task* task, const char* feedback) {
    char payload_path[1024], context_path[1024], style_path[1024], target_path[1024], ledger_path[1024];
    snprintf(payload_path, sizeof(payload_path), "%s/%s", project_path, PAYLOAD_NAME);
    snprintf(context_path, sizeof(context_path), "%s/context.md", project_path);
    snprintf(style_path, sizeof(style_path), "%s/templates/ui_guidelines.md", project_path);
    snprintf(target_path, sizeof(target_path), "%s/%s", project_path, task->file);
    snprintf(ledger_path, sizeof(ledger_path), "%s/tasks.txt", project_path);

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
    fprintf(payload, "### ALREADY BUILT ###\n");
    ledger_write_built_summary(payload, ledger_path);
    fprintf(payload, "\n### CURRENT GOAL ###\n%s\n\n", task->goal);
    write_reads(payload, project_path, task->reads);
    write_ui_resources(payload, project_path, task->ui);
    fprintf(payload, "### CURRENT CONTENT OF %s ###\n%s\n\n", task->file, current_text ? current_text : "(new file)");
    fprintf(payload, "### TASK ###\n%s\n", task->description);
    if (feedback[0] != '\0') {
        fprintf(payload, "\n### PREVIOUS ATTEMPT FAILED ###\n%s\n", feedback);
    }
    fprintf(payload, "\nRemember: reply with ONLY the complete content of `%s` in one fenced code block.\n",
            task->file);

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

// Explains why a reply had no usable code block, quoting how the reply starts
// so errors such as "model requires more memory" are visible right away.
void describe_missing_block(const char* output, char* reason, size_t reason_size,
                            char* feedback, size_t feedback_size) {
    if (!output || output[strspn(output, " \t\r\n")] == '\0') {
        snprintf(reason, reason_size, "empty reply from the model (is Ollama running and the model downloaded?)");
    } else {
        char excerpt[121];
        size_t start = strspn(output, " \t\r\n");
        size_t length = strcspn(output + start, "\r\n");
        if (length > 120) length = 120;
        memcpy(excerpt, output + start, length);
        excerpt[length] = '\0';
        snprintf(reason, reason_size, "no complete code block in the reply. It starts with: %s", excerpt);
    }
    snprintf(feedback, feedback_size,
             "Your reply did not contain a complete fenced code block with the file content.");
}

// Verification only runs against real model output; simulated output is placeholder text.
int verify_task(const char* project_path, const Task* task, char* feedback, size_t feedback_size,
                char* reason, size_t reason_size) {
    if (task->verify[0] == '\0' || !OLLAMA_ENABLED) return 1;

    char command[2048];
    snprintf(command, sizeof(command), VERIFY_COMMAND_FORMAT, project_path, task->verify, VERIFY_OUTPUT_NAME);
    int code = run_shell(command, VERIFY_TIMEOUT_SECONDS);

    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s", project_path, VERIFY_OUTPUT_NAME);

    if (code != 0) {
        char* output = read_file(output_path);
        const char* tail = output ? output : "";
        size_t length = strlen(tail);
        if (length > 1500) tail += length - 1500;

        if (code == 124) {
            snprintf(reason, reason_size, "VERIFY timed out after %d seconds", VERIFY_TIMEOUT_SECONDS);
            snprintf(feedback, feedback_size,
                     "The verification command `%s` did not finish within %d seconds. Last output:\n%s",
                     task->verify, VERIFY_TIMEOUT_SECONDS, tail);
        } else {
            snprintf(reason, reason_size, "VERIFY command failed (exit code %d)", code);
            snprintf(feedback, feedback_size, "The verification command `%s` failed with this output:\n%s",
                     task->verify, tail);
        }
        free(output);
    }
    remove(output_path);
    return code == 0;
}

// ==========================================
//      Logs
// ==========================================
// Saves everything needed to understand a failed attempt.
void save_log(const char* log_dir, const char* label, int attempt, const char* reason,
              const char* details, const char* model_output) {
    MKDIR(log_dir);

    char safe[200];
    snprintf(safe, sizeof(safe), "%s", label);
    for (char* c = safe; *c; c++) {
        if (*c == '/' || *c == '\\' || *c == ' ') *c = '_';
    }

    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", localtime(&now));

    char path[1600];
    snprintf(path, sizeof(path), "%s/%s_%s_attempt%d.log", log_dir, stamp, safe, attempt);

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "Label: %s\nAttempt: %d\nReason: %s\n\n--- Details ---\n%s\n\n--- Model reply ---\n%s\n",
            label, attempt, reason, details ? details : "", model_output ? model_output : "(no reply)");
    fclose(f);
}

void log_progress(const char* project_path, const Task* task, int attempts) {
    FILE *f = open_project_file(project_path, "progress_log.md", "a");
    if (!f) return;

    char stamp[32];
    time_t now = time(NULL);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", localtime(&now));

    if (attempts > 0) {
        fprintf(f, "- %s [done] %s (attempts: %d): %s\n", stamp, task->file, attempts, task->description);
    } else {
        fprintf(f, "- %s [FAILED] %s: %s\n", stamp, task->file, task->description);
    }
    fclose(f);
}

void write_attention_file(const char* project_path, const Task* task) {
    FILE *f = open_project_file(project_path, ATTENTION_FILE_NAME, "w");
    if (!f) return;

    fprintf(f,
            "# This project needs your attention\n\n"
            "A task failed after %d attempts and the run was stopped.\n\n"
            "- File: %s\n- Task: %s\n\n"
            "The reply of every attempt and the verification output are in the logs/ folder.\n"
            "Fix the cause (edit the files, the task in tasks.txt, or the model in Settings),\n"
            "then run the agent again. It resumes from this task.\n",
            MAX_ATTEMPTS, task->file, task->description);
    fclose(f);
}

// ==========================================
//      Task Execution
// ==========================================
// Returns the number of attempts used on success, 0 if every attempt failed and
// -1 if the run was paused. Unless it succeeds, the target file is left exactly as it was.
int execute_task(const char* project_path, const char* model, const Task* task) {
    char target_path[1024], payload_path[1024], log_dir[1024];
    snprintf(target_path, sizeof(target_path), "%s/%s", project_path, task->file);
    snprintf(payload_path, sizeof(payload_path), "%s/%s", project_path, PAYLOAD_NAME);
    snprintf(log_dir, sizeof(log_dir), "%s/logs", project_path);

    char* original = read_file(target_path);
    char feedback[2048] = "";
    int result = 0;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS && result == 0; attempt++) {
        if (!write_payload(project_path, task, feedback)) {
            printf("%s could not write the payload file\n", ui_strings[62]);
            break;
        }

        printf("%s\n", ui_strings[13]);
        char* output = run_model(model, payload_path, task->file);
        printf("%s\n", ui_strings[14]);
        if (pause_requested) {
            free(output);
            result = -1;
            break;
        }
        unload_model(model);

        char reason[512] = "";
        int verified = 0;
        char* code = output ? extract_code_block(output) : NULL;
        if (code) {
            write_text_file(target_path, code);
            free(code);
            verified = verify_task(project_path, task, feedback, sizeof(feedback), reason, sizeof(reason));
        } else {
            describe_missing_block(output, reason, sizeof(reason), feedback, sizeof(feedback));
        }

        if (pause_requested) {
            result = -1;
        } else if (verified) {
            result = attempt;
        } else {
            printf("%s [%d/%d] %s\n", ui_strings[62], attempt, MAX_ATTEMPTS, reason);
            save_log(log_dir, task->file, attempt, reason, feedback, output);
        }
        free(output);
    }

    if (result <= 0) {
        if (original) write_text_file(target_path, original);
        else remove(target_path);
    }

    free(original);
    remove(payload_path);
    return result;
}

// ==========================================
//      Orchestrator Loop
//      Works through the ledger without supervision. Stops when a task fails
//      all its attempts (later tasks depend on it) or when a pause is requested
//      with Ctrl+C or a PAUSE file in the project folder. Run it again to resume.
// ==========================================
RunResult run_agent(const char* project_path, const char* model) {
    char ledger_path[1024], pause_path[1024], attention_path[1024], log_dir[1024];
    snprintf(ledger_path, sizeof(ledger_path), "%s/tasks.txt", project_path);
    snprintf(pause_path, sizeof(pause_path), "%s/%s", project_path, PAUSE_FILE_NAME);
    snprintf(attention_path, sizeof(attention_path), "%s/%s", project_path, ATTENTION_FILE_NAME);
    snprintf(log_dir, sizeof(log_dir), "%s/logs", project_path);

    if (!file_exists(ledger_path)) {
        printf("%s\n", ui_strings[11]);
        return RUN_ERROR;
    }
    if (!ensure_ollama()) return RUN_ERROR;

    printf("%s\n", ui_strings[32]);
    ensure_model(model);
    ledger_reset_failed(ledger_path);
    remove(attention_path);
    remove(pause_path);

    int done = 0;
    int total = 0;
    ledger_count(ledger_path, &done, &total);
    if (done > 0) printf("%s %d/%d\n", ui_strings[61], done, total);

    pause_requested = 0;
    signal(SIGINT, request_pause);

    RunResult result = RUN_FINISHED;
    int completed = 0;
    Task task;
    memset(&task, 0, sizeof(task));

    while (result == RUN_FINISHED && ledger_next_pending(ledger_path, &task)) {
        if (pause_requested || file_exists(pause_path)) {
            remove(pause_path);
            result = RUN_PAUSED;
            break;
        }

        printf("\n%s %s\n", ui_strings[12], task.description);
        int attempts = execute_task(project_path, model, &task);

        if (attempts < 0) {
            result = RUN_PAUSED;
            continue;
        }

        ledger_mark(ledger_path, task.ledger_offset, attempts > 0 ? 'x' : '!');
        log_progress(project_path, &task, attempts);

        if (attempts > 0) {
            completed++;
            printf("%s %s\n", ui_strings[29], task.file);
        } else {
            printf("%s %s\n", ui_strings[30], task.file);
            write_attention_file(project_path, &task);
            result = RUN_STOPPED;
        }
    }

    signal(SIGINT, SIG_DFL);
    pause_requested = 0;

    if (result == RUN_PAUSED) {
        unload_model(model);
        printf("\n%s\n", ui_strings[60]);
    } else if (result == RUN_STOPPED) {
        printf("\a\n%s %s\n%s\n", ui_strings[58], log_dir, ui_strings[59]);
    } else {
        printf("\n%s (ok: %d)\n", ui_strings[31], completed);
    }
    return result;
}

#endif