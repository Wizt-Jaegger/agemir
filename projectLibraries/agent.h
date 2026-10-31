#ifndef AGENT_H
#define AGENT_H

#include "project.h"

#define MAX_ATTEMPTS 3
#define MAX_INCLUDED_BYTES 4000
#define PAYLOAD_NAME ".agemir_payload.txt"
#define VERIFY_OUTPUT_NAME ".agemir_verify.txt"

// ==========================================
//      Ollama Control
// ==========================================
void ensure_model(const char* model) {
#if OLLAMA_ENABLED
    char command[512];
    snprintf(command, sizeof(command), "ollama pull %s %s", model, DEV_NULL);
    int status = system(command);
    (void)status;
#else
    (void)model;
#endif
}

// Frees the RAM held by the model so the next task starts from a clean machine.
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

    char* output = malloc(strlen(target_file) + 64);
    if (output) snprintf(output, strlen(target_file) + 64, "```\nSimulated output for %s\n```\n", target_file);
    return output;
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
    fprintf(payload, "### CURRENT CONTENT OF %s ###\n%s\n\n", task->file, current_text ? current_text : "(new file)");
    fprintf(payload, "### TASK ###\n%s\n", task->description);
    if (feedback[0] != '\0') {
        fprintf(payload, "\n### PREVIOUS ATTEMPT FAILED ###\n%s\n", feedback);
    }

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

// Verification only runs against real model output; simulated output is placeholder text.
int verify_task(const char* project_path, const Task* task, char* feedback, size_t feedback_size) {
    if (task->verify[0] == '\0' || !OLLAMA_ENABLED) return 1;

    char command[2048];
    snprintf(command, sizeof(command), VERIFY_COMMAND_FORMAT, project_path, task->verify, VERIFY_OUTPUT_NAME);
    int status = system(command);

    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s", project_path, VERIFY_OUTPUT_NAME);

    if (status != 0) {
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

// Returns the number of attempts used on success, 0 if the task failed.
// A failed task leaves the target file exactly as it was before.
int execute_task(const char* project_path, const char* model, const Task* task) {
    char target_path[1024], payload_path[1024];
    snprintf(target_path, sizeof(target_path), "%s/%s", project_path, task->file);
    snprintf(payload_path, sizeof(payload_path), "%s/%s", project_path, PAYLOAD_NAME);

    char* original = read_file(target_path);
    char feedback[2048] = "";
    int attempts_used = 0;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS && attempts_used == 0; attempt++) {
        if (!write_payload(project_path, task, feedback)) break;

        printf("%s\n", ui_strings[13]);
        char* output = run_model(model, payload_path, task->file);
        unload_model(model);
        printf("%s\n", ui_strings[14]);

        char* code = output ? extract_code_block(output) : NULL;
        free(output);
        if (!code) {
            snprintf(feedback, sizeof(feedback),
                     "Your reply did not contain a complete fenced code block with the file content.");
            continue;
        }

        write_text_file(target_path, code);
        free(code);
        if (verify_task(project_path, task, feedback, sizeof(feedback))) {
            attempts_used = attempt;
        }
    }

    if (attempts_used == 0) {
        if (original) write_text_file(target_path, original);
        else remove(target_path);
    }

    free(original);
    remove(payload_path);
    return attempts_used;
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

// ==========================================
//      Orchestrator Loop
// ==========================================
int run_agent(const char* project_path, const char* model) {
    char ledger_path[1024];
    snprintf(ledger_path, sizeof(ledger_path), "%s/tasks.txt", project_path);

    if (!file_exists(ledger_path)) {
        printf("%s\n", ui_strings[11]);
        return 0;
    }
    if (OLLAMA_ENABLED && system("ollama --version" DEV_NULL) != 0) {
        printf("%s %s\n", ui_strings[33], INSTALL_OLLAMA);
        return 0;
    }

    printf("%s\n", ui_strings[32]);
    ensure_model(model);
    ledger_reset_failed(ledger_path);

    int completed = 0;
    int failed = 0;
    Task task;
    memset(&task, 0, sizeof(task));

    while (ledger_next_pending(ledger_path, &task)) {
        printf("\n%s %s\n", ui_strings[12], task.description);

        int attempts = execute_task(project_path, model, &task);
        ledger_mark(ledger_path, task.ledger_offset, attempts > 0 ? 'x' : '!');
        log_progress(project_path, &task, attempts);

        if (attempts > 0) {
            completed++;
            printf("%s %s\n", ui_strings[29], task.file);
        } else {
            failed++;
            printf("%s %s\n", ui_strings[30], task.file);
        }
    }

    printf("\n%s (ok: %d, failed: %d)\n", ui_strings[31], completed, failed);
    return 1;
}

#endif