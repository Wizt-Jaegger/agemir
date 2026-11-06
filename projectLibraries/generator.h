#ifndef GENERATOR_H
#define GENERATOR_H

#include "agent.h"

#define MAX_DESCRIPTION_BYTES 4000
#define GENERATOR_PAYLOAD_NAME ".agemir_gen_payload.txt"
#define GENERATOR_STDERR_NAME ".agemir_gen_stderr.txt"

// Reads lines until the user sends an empty one.
void read_description(char* buffer, size_t size) {
    buffer[0] = '\0';
    char line[512];

    while (1) {
        read_line(line, sizeof(line));
        if (line[0] == '\0') break;
        if (strlen(buffer) + strlen(line) + 2 < size) {
            strcat(buffer, line);
            strcat(buffer, "\n");
        }
    }
}

// File names found in the UI templates folder (info.txt excluded), separated by commas.
void list_ui_templates(char* out, size_t size) {
    out[0] = '\0';

    char command[1024];
    snprintf(command, sizeof(command), LIST_DIR_FORMAT, UI_TEMPLATES_DIR);
    FILE *pipe = POPEN(command, "r");
    if (!pipe) return;

    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || strcmp(line, "info.txt") == 0) continue;
        if (strlen(out) + strlen(line) + 3 >= size) break;

        if (out[0] != '\0') strcat(out, ",");
        strcat(out, line);
    }
    PCLOSE(pipe);
}

// The user chose the project name, so the template always uses exactly that name.
char* force_project_name(const char* text, const char* name) {
    size_t capacity = strlen(text) + strlen(name) + 32;
    char* result = malloc(capacity);
    if (!result) return NULL;

    size_t written = (size_t)snprintf(result, capacity, "PROJECT_NAME: %s\n", name);
    const char* line = text;

    while (*line) {
        const char* end = strchr(line, '\n');
        size_t length = end ? (size_t)(end - line) + 1 : strlen(line);

        const char* probe = line;
        while (probe < line + length && (*probe == ' ' || *probe == '\t')) probe++;
        if (strncmp(probe, "PROJECT_NAME", 12) != 0) {
            memcpy(result + written, line, length);
            written += length;
        }
        line += length;
    }
    result[written] = '\0';
    return result;
}

int write_generator_payload(const char* payload_path, const char* name, const char* description,
                            int graphical, const char* feedback) {
    FILE *payload = fopen(payload_path, "w");
    if (!payload) return 0;

    char* guide = read_file_capped(TEMPLATE_SOURCE, MAX_INCLUDED_BYTES);
    char* example = read_file_capped(EXAMPLE_TEMPLATE_PATH, MAX_INCLUDED_BYTES);

    fprintf(payload,
            "SYSTEM: You write project templates for Agemir, a tool where a small local AI model builds a project "
            "one small task at a time.\n"
            "Reply with ONLY the complete template inside a single fenced code block. Follow the FORMAT GUIDE exactly.\n"
            "Rules:\n"
            "- PROJECT_NAME must be %s.\n"
            "- Elephant memory: list every requested outcome as a numbered OBJECTIVE (1023 bytes total). Use RULES for constraints "
            "and STYLE for a compact shared contract (paths, exact signatures, data shapes and entry point).\n"
            "- Goldfish tasks: give each task one behavior and one small file, or one RUN command. "
            "Split large features into modules a small coding model can generate in one reply. "
            "Name the requirement and the expected result in the TASK description (max 300 characters).\n"
            "- Order work from shared interfaces to features to integration. READS may name only earlier FILE "
            "outputs and should list just the direct dependencies.\n"
            "- Use VERIFY for executable acceptance checks, including expected behavior when possible. "
            "Add a final RUN task that runs the end-to-end checks against the requested outcomes. "
            "Use commands supported by the requested stack and operating system.\n\n",
            name);
    fprintf(payload, "### FORMAT GUIDE ###\n%s\n\n### EXAMPLE OF A GOOD TEMPLATE ###\n%s\n\n",
            guide ? guide : "", example ? example : "");

    if (graphical) {
        char templates[512];
        list_ui_templates(templates, sizeof(templates));
        fprintf(payload,
                "### THIS IS A GRAPHICAL PROJECT ###\n"
                "Available UI templates: %s\n"
                "For tasks that build user interface pieces add a UI line with names taken ONLY from that list, "
                "for example UI: navbar.md,login-menu.md. If the list is empty, do not use UI.\n\n",
                templates[0] != '\0' ? templates : "(none)");
    }

    fprintf(payload, "### PROJECT DESCRIPTION ###\n%s\n", description);
    if (feedback[0] != '\0') {
        fprintf(payload, "\n### YOUR PREVIOUS TEMPLATE WAS REJECTED ###\n%s\n", feedback);
    }
    fprintf(payload, "\nREMINDER: reply with ONLY the template in one fenced code block.\n");

    free(guide);
    free(example);
    fclose(payload);
    return 1;
}

// These checks apply to generated templates. Imported templates keep their existing format.
int validate_generated_plan(const Project* project, char* error, size_t error_size) {
    if (!project->style || !project->style[0] || strlen(project->style) > MAX_INCLUDED_BYTES) {
        snprintf(error, error_size, "STYLE must contain a shared contract within %d bytes", MAX_INCLUDED_BYTES);
        return 0;
    }
    for (int i = 0; i < project->task_count; i++) {
        const Task* task = &project->tasks[i];
        if (!task->goal[0] || !task->description[0] || strlen(task->description) > 300) {
            snprintf(error, error_size, "task %d needs a GOAL and a description of 1 to 300 bytes", i + 1);
            return 0;
        }
        char list[sizeof(task->reads)];
        snprintf(list, sizeof(list), "%s", task->reads);
        for (char* path = strtok(list, ","); path; path = strtok(NULL, ",")) {
            path = trim(path);
            int previous = 0;
            while (previous < i && strcmp(project->tasks[previous].file, path) != 0) previous++;
            if (previous == i) {
                snprintf(error, error_size, "task %d: READS must name FILE outputs from earlier tasks", i + 1);
                return 0;
            }
        }
    }
    if (!project->tasks[project->task_count - 1].is_command) {
        snprintf(error, error_size, "end with a RUN task that checks the integrated project");
        return 0;
    }
    return 1;
}

// Asks the model to write the template, validates it, and creates the project from it.
// The template is saved as <name>_gen_template.txt inside the project folder.
int generate_project(const char* model, const char* name, const char* project_path,
                     const char* description, int graphical) {
    if (!ensure_ollama_ready() || !ensure_model(model)) return 0;

    char resolved[1024];
    MKDIR(project_path);
    resolve_path(project_path, resolved, sizeof(resolved));

    char payload_path[2048], stderr_path[2048], template_path[2200], file_name[128];
    join_path(payload_path, sizeof(payload_path), resolved, GENERATOR_PAYLOAD_NAME);
    join_path(stderr_path, sizeof(stderr_path), resolved, GENERATOR_STDERR_NAME);
    snprintf(file_name, sizeof(file_name), "%s_gen_template.txt", name);
    join_path(template_path, sizeof(template_path), resolved, file_name);

    char feedback[512] = "";
    int valid = 0;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS && !valid; attempt++) {
        printf("%s %d/%d\n", ui_strings[57], attempt, MAX_ATTEMPTS);
        if (!write_generator_payload(payload_path, name, description, graphical, feedback)) break;

        char* output = run_model(model, payload_path, stderr_path, GENERATOR_TARGET);
        unload_model(model);
        char* errors = read_file(stderr_path);
        char* code = output ? extract_code_block(output) : NULL;
        int model_failed = output == NULL;
        free(output);

        if (!code) {
            char error_line[256];
            int fatal = first_error_line(errors, error_line, sizeof(error_line));
            free(errors);
            if (fatal || model_failed) {
                printf("%s %s\n%s\n", ui_strings[55], fatal ? error_line : "could not run ollama", ui_strings[56]);
                break;
            }
            snprintf(feedback, sizeof(feedback), "Your reply did not contain a complete fenced code block.");
            continue;
        }
        free(errors);

        char* forced = force_project_name(code, name);
        free(code);
        if (!forced) continue;
        int saved = write_text_file(template_path, forced);
        free(forced);
        if (!saved) break;

        Project* project = calloc(1, sizeof(Project));
        if (!project) break;
        char error[256] = "";
        valid = parse_template(template_path, project, error, sizeof(error)) &&
                validate_generated_plan(project, error, sizeof(error));
        free(project->style);
        free(project);

        if (!valid) {
            snprintf(feedback, sizeof(feedback), "%s", error);
            printf("%s %s\n", ui_strings[27], error);
        }
    }

    remove(payload_path);
    remove(stderr_path);

    if (!valid) {
        printf("%s\n", ui_strings[62]);
        if (file_exists(template_path)) printf("%s\n", template_path);
        return 0;
    }

    printf("%s %s\n", ui_strings[23], template_path);
    return import_project(template_path, resolved);
}

#endif