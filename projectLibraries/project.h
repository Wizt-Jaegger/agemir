#ifndef PROJECT_H
#define PROJECT_H

#include "utils.h"

#define MAX_TASKS 256

typedef struct {
    char goal[128];
    char description[512];
    char file[256];
    char reads[512];
    char verify[512];
    char ui[256];
    long ledger_offset;
} Task;

typedef struct {
    char name[64];
    char objective[1024];
    char tech_stack[512];
    char rules[1024];
    char* style;
    int task_count;
    Task tasks[MAX_TASKS];
} Project;

// ==========================================
//      Template Parsing
// ==========================================
void append_field(char* field, size_t size, const char* value) {
    size_t length = strlen(field);
    if (length > 0 && length + 1 < size) {
        field[length++] = '\n';
        field[length] = '\0';
    }
    snprintf(field + length, size - length, "%s", value);
}

void append_dynamic(char** target, const char* line) {
    size_t old_length = *target ? strlen(*target) : 0;
    size_t added = strlen(line) + 2;
    char* grown = realloc(*target, old_length + added);
    if (!grown) return;

    snprintf(grown + old_length, added, "%s\n", line);
    *target = grown;
}

int valid_reads_list(const char* reads) {
    char list[512];
    snprintf(list, sizeof(list), "%s", reads);
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        if (!is_safe_relative_path(trim(name))) return 0;
    }
    return 1;
}

// UI templates are plain file names that must exist in resources/UI_templates_Agents.
int valid_ui_list(const char* ui, char* missing, size_t missing_size) {
    char list[256];
    snprintf(list, sizeof(list), "%s", ui);
    for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
        name = trim(name);

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", UI_TEMPLATES_DIR, name);
        int plain_name = name[0] != '\0' && strpbrk(name, "/\\") == NULL && strstr(name, "..") == NULL;

        if (!plain_name || strcmp(name, "info.txt") == 0 || !file_exists(path)) {
            snprintf(missing, missing_size, "%s", name);
            return 0;
        }
    }
    return 1;
}

int validate_project(const Project* project, char* error, size_t error_size) {
    if (!is_valid_name(project->name)) {
        snprintf(error, error_size, "PROJECT_NAME must use only letters, numbers, '_' and '-'");
        return 0;
    }
    if (project->objective[0] == '\0') {
        snprintf(error, error_size, "OBJECTIVE is empty");
        return 0;
    }
    if (project->tech_stack[0] == '\0') {
        snprintf(error, error_size, "TECH_STACK is empty");
        return 0;
    }
    if (project->task_count == 0) {
        snprintf(error, error_size, "no TASK found");
        return 0;
    }

    for (int i = 0; i < project->task_count; i++) {
        const Task* task = &project->tasks[i];
        char missing[256];
        if (!is_safe_relative_path(task->file)) {
            snprintf(error, error_size, "task %d: FILE must be a relative path inside the project", i + 1);
            return 0;
        }
        if (!valid_reads_list(task->reads)) {
            snprintf(error, error_size, "task %d: READS must be relative paths separated by commas", i + 1);
            return 0;
        }
        if (!valid_ui_list(task->ui, missing, sizeof(missing))) {
            snprintf(error, error_size, "task %d: UI template '%s' not found in %s", i + 1, missing, UI_TEMPLATES_DIR);
            return 0;
        }
    }
    return 1;
}

int parse_template(const char* path, Project* project, char* error, size_t error_size) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(error, error_size, "could not read %s", path);
        return 0;
    }

    char line[2048];
    char goal[128] = "";
    int in_style = 0;
    int line_number = 0;
    int ok = 1;

    while (ok && fgets(line, sizeof(line), f)) {
        line_number++;
        line[strcspn(line, "\r\n")] = '\0';

        if (in_style) {
            char probe[2048];
            snprintf(probe, sizeof(probe), "%s", line);
            if (strcmp(trim(probe), "END_STYLE") == 0) {
                in_style = 0;
            } else {
                append_dynamic(&project->style, line);
            }
            continue;
        }

        char* text = trim(line);
        if (text[0] == '\0' || text[0] == '#') continue;

        char* colon = strchr(text, ':');
        if (!colon) {
            snprintf(error, error_size, "line %d: expected FIELD: value", line_number);
            ok = 0;
            break;
        }

        *colon = '\0';
        char* key = trim(text);
        char* value = trim(colon + 1);
        for (char* c = value; *c; c++) {
            if (*c == '\t') *c = ' ';
        }

        if (strcmp(key, "PROJECT_NAME") == 0) {
            snprintf(project->name, sizeof(project->name), "%s", value);
        } else if (strcmp(key, "OBJECTIVE") == 0) {
            append_field(project->objective, sizeof(project->objective), value);
        } else if (strcmp(key, "TECH_STACK") == 0) {
            append_field(project->tech_stack, sizeof(project->tech_stack), value);
        } else if (strcmp(key, "RULES") == 0) {
            append_field(project->rules, sizeof(project->rules), value);
        } else if (strcmp(key, "STYLE") == 0) {
            in_style = 1;
        } else if (strcmp(key, "GOAL") == 0) {
            snprintf(goal, sizeof(goal), "%s", value);
        } else if (strcmp(key, "TASK") == 0) {
            if (goal[0] == '\0') {
                snprintf(error, error_size, "line %d: TASK before any GOAL", line_number);
                ok = 0;
            } else if (project->task_count >= MAX_TASKS) {
                snprintf(error, error_size, "too many tasks (max %d)", MAX_TASKS);
                ok = 0;
            } else {
                Task* task = &project->tasks[project->task_count++];
                snprintf(task->goal, sizeof(task->goal), "%s", goal);
                snprintf(task->description, sizeof(task->description), "%s", value);
            }
        } else if (strcmp(key, "FILE") == 0 || strcmp(key, "READS") == 0 ||
                   strcmp(key, "VERIFY") == 0 || strcmp(key, "UI") == 0) {
            if (project->task_count == 0) {
                snprintf(error, error_size, "line %d: %s before any TASK", line_number, key);
                ok = 0;
            } else {
                Task* task = &project->tasks[project->task_count - 1];
                if (strcmp(key, "FILE") == 0) {
                    snprintf(task->file, sizeof(task->file), "%s", value);
                } else if (strcmp(key, "READS") == 0) {
                    snprintf(task->reads, sizeof(task->reads), "%s", value);
                } else if (strcmp(key, "UI") == 0) {
                    snprintf(task->ui, sizeof(task->ui), "%s", value);
                } else {
                    snprintf(task->verify, sizeof(task->verify), "%s", value);
                }
            }
        } else {
            snprintf(error, error_size, "line %d: unknown field '%s'", line_number, key);
            ok = 0;
        }
    }
    fclose(f);

    if (ok && in_style) {
        snprintf(error, error_size, "STYLE block is never closed with END_STYLE");
        ok = 0;
    }
    return ok && validate_project(project, error, error_size);
}

// ==========================================
//      Project Files
// ==========================================
FILE* open_project_file(const char* project_path, const char* name, const char* mode) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", project_path, name);
    return fopen(path, mode);
}

// Copies the UI templates the tasks use, and the icon/image links, into the project.
void copy_ui_resources(const Project* project, const char* project_path) {
    int uses_ui = 0;

    for (int i = 0; i < project->task_count; i++) {
        char list[256];
        snprintf(list, sizeof(list), "%s", project->tasks[i].ui);
        for (char* name = strtok(list, ","); name; name = strtok(NULL, ",")) {
            name = trim(name);
            char source[512], destination[1100];
            snprintf(source, sizeof(source), "%s/%s", UI_TEMPLATES_DIR, name);
            snprintf(destination, sizeof(destination), "%s/templates/ui/%s", project_path, name);
            make_parent_dirs(destination);
            copy_file(source, destination);
            uses_ui = 1;
        }
    }

    if (uses_ui && file_exists(INTERNET_SOURCES_PATH)) {
        char destination[1100];
        snprintf(destination, sizeof(destination), "%s/templates/internet_sources.txt", project_path);
        copy_file(INTERNET_SOURCES_PATH, destination);
    }
}

int write_project_files(const Project* project, const char* template_path,
                        const char* project_path, char* error, size_t error_size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/tasks.txt", project_path);
    if (file_exists(path)) {
        snprintf(error, error_size, "%s already contains a project (tasks.txt exists)", project_path);
        return 0;
    }

    snprintf(path, sizeof(path), "%s/templates", project_path);
    MKDIR(path);

    FILE *context = open_project_file(project_path, "context.md", "w");
    FILE *style = open_project_file(project_path, "templates/ui_guidelines.md", "w");
    FILE *ledger = open_project_file(project_path, "tasks.txt", "w");
    FILE *progress_log = open_project_file(project_path, "progress_log.md", "w");

    if (!context || !style || !ledger || !progress_log) {
        if (context) fclose(context);
        if (style) fclose(style);
        if (ledger) fclose(ledger);
        if (progress_log) fclose(progress_log);
        snprintf(error, error_size, "could not write files in %s", project_path);
        return 0;
    }

    fprintf(context, "# %s\n\n## Objective\n%s\n\n## Tech Stack\n%s\n\n## Rules\n%s\n\n## Planned Files\n",
            project->name, project->objective, project->tech_stack, project->rules);
    for (int i = 0; i < project->task_count; i++) {
        fprintf(context, "- %s: %s\n", project->tasks[i].file, project->tasks[i].description);
    }

    fputs(project->style ? project->style : "No style guidelines provided.\n", style);

    const char* last_goal = "";
    for (int i = 0; i < project->task_count; i++) {
        const Task* task = &project->tasks[i];
        if (strcmp(task->goal, last_goal) != 0) {
            fprintf(ledger, "# %s\n", task->goal);
            last_goal = task->goal;
        }
        fprintf(ledger, "[ ]\t%s\t%s\t%s\t%s\t%s\n",
                task->file, task->reads, task->verify, task->ui, task->description);
    }

    fprintf(progress_log, "# Progress Log\n\n");

    fclose(context);
    fclose(style);
    fclose(ledger);
    fclose(progress_log);

    snprintf(path, sizeof(path), "%s/template.txt", project_path);
    copy_file(template_path, path);
    copy_ui_resources(project, project_path);
    return 1;
}

int import_project(const char* template_path, const char* project_path) {
    Project* project = calloc(1, sizeof(Project));
    if (!project) return 0;

    char error[256] = "";
    char resolved[1024];
    int ok = parse_template(template_path, project, error, sizeof(error));

    if (ok) {
        MKDIR(project_path);
        resolve_path(project_path, resolved, sizeof(resolved));
        ok = write_project_files(project, template_path, resolved, error, sizeof(error));
    }

    if (ok) {
        config_register_project(project->name, resolved);
        printf("%s %s -> %s\n", ui_strings[28], project->name, resolved);
    } else {
        printf("%s %s\n", ui_strings[27], error);
    }

    free(project->style);
    free(project);
    return ok;
}

// ==========================================
//      Task Ledger (tasks.txt)
//      "# Goal title" lines group tasks.
//      Task lines: [ ] pending, [x] done, [!] failed, then tab-separated
//      file, reads, verify, ui and description.
//      Older ledgers without the ui column are still read.
// ==========================================
int parse_ledger_line(char* line, Task* task) {
    char* fields[6];
    int count = 0;
    char* cursor = line;

    fields[count++] = cursor;
    while (count < 6 && (cursor = strchr(cursor, '\t')) != NULL) {
        *cursor++ = '\0';
        fields[count++] = cursor;
    }
    if (count < 5) return 0;

    snprintf(task->file, sizeof(task->file), "%s", fields[1]);
    snprintf(task->reads, sizeof(task->reads), "%s", fields[2]);
    snprintf(task->verify, sizeof(task->verify), "%s", fields[3]);
    if (count == 6) {
        snprintf(task->ui, sizeof(task->ui), "%s", fields[4]);
        snprintf(task->description, sizeof(task->description), "%s", fields[5]);
    } else {
        task->ui[0] = '\0';
        snprintf(task->description, sizeof(task->description), "%s", fields[4]);
    }
    return 1;
}

int ledger_next_pending(const char* ledger_path, Task* task) {
    FILE *f = fopen(ledger_path, "rb");
    if (!f) return 0;

    char line[4096];
    char goal[128] = "";
    int found = 0;

    while (!found) {
        long line_start = ftell(f);
        if (!fgets(line, sizeof(line), f)) break;
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '#') {
            snprintf(goal, sizeof(goal), "%s", trim(line + 1));
        } else if (strncmp(line, "[ ]", 3) == 0 && parse_ledger_line(line, task)) {
            snprintf(task->goal, sizeof(task->goal), "%s", goal);
            task->ledger_offset = line_start;
            found = 1;
        }
    }
    fclose(f);
    return found;
}

void ledger_mark(const char* ledger_path, long offset, char marker) {
    FILE *f = fopen(ledger_path, "r+b");
    if (!f) return;
    fseek(f, offset + 1, SEEK_SET);
    fputc(marker, f);
    fclose(f);
}

void ledger_reset_failed(const char* ledger_path) {
    long offsets[MAX_TASKS];
    int count = 0;

    FILE *f = fopen(ledger_path, "rb");
    if (!f) return;

    char line[4096];
    while (count < MAX_TASKS) {
        long line_start = ftell(f);
        if (!fgets(line, sizeof(line), f)) break;
        if (strncmp(line, "[!]", 3) == 0) offsets[count++] = line_start;
    }
    fclose(f);

    for (int i = 0; i < count; i++) {
        ledger_mark(ledger_path, offsets[i], ' ');
    }
}

void ledger_count(const char* ledger_path, int* done, int* total) {
    *done = 0;
    *total = 0;

    FILE *f = fopen(ledger_path, "rb");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "[x]", 3) == 0) {
            (*done)++;
            (*total)++;
        } else if (strncmp(line, "[ ]", 3) == 0 || strncmp(line, "[!]", 3) == 0) {
            (*total)++;
        }
    }
    fclose(f);
}

void ledger_write_built_summary(FILE* out, const char* ledger_path) {
    FILE *f = fopen(ledger_path, "rb");
    if (!f) return;

    char line[4096];
    int any = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        Task task;
        if (strncmp(line, "[x]", 3) == 0 && parse_ledger_line(line, &task)) {
            fprintf(out, "- %s: %s\n", task.file, task.description);
            any = 1;
        }
    }
    fclose(f);

    if (!any) fprintf(out, "(nothing yet)\n");
}

#endif