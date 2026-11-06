#ifndef UTILS_H
#define UTILS_H

#if !defined(_WIN32)
#define _XOPEN_SOURCE 700
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>

// Set to 1 (or compile with -DOLLAMA_ENABLED=1) to call Ollama for real.
// With 0 the model output is simulated so the whole workflow can be tested without Ollama.
#ifndef OLLAMA_ENABLED
#define OLLAMA_ENABLED 0
#endif

// ==========================================
//      Macros & Global OS Assignment
// ==========================================
#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define OS_NAME "Windows"
#define MKDIR(path) _mkdir(path)
#define POPEN _popen
#define PCLOSE _pclose
#define START_OLLAMA_SERVER "start \"\" /B ollama.exe serve < NUL > NUL 2>&1"
#define LIST_DIR_FORMAT "dir /b \"%s\" 2> NUL"
#define DEV_NULL " > NUL 2>&1"
#define CLEAN "cls"
#define VERIFY_COMMAND_FORMAT "cd /d \"%s\" && (%s) > \"%s\" 2>&1"
#ifndef INSTALL_OLLAMA
#define INSTALL_OLLAMA "winget install --id Ollama.Ollama --exact"
#endif
#else
#include <unistd.h>
#include <sys/wait.h>
#define OS_NAME "Linux/macOS"
#define MKDIR(path) mkdir(path, 0777)
#define POPEN popen
#define PCLOSE pclose
#define START_OLLAMA_SERVER "ollama serve > /dev/null 2>&1 &"
#define LIST_DIR_FORMAT "ls -1 \"%s\" 2>/dev/null"
#define DEV_NULL " > /dev/null 2>&1"
#define CLEAN "clear"
#define VERIFY_COMMAND_FORMAT "cd \"%s\" && (%s) > \"%s\" 2>&1"
#ifndef INSTALL_OLLAMA
#define INSTALL_OLLAMA "curl -fsSL https://ollama.com/install.sh | sh"
#endif
#endif

#define CONFIG_DIR "resources/config"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
#define CONFIG_LINE 1024
#define MODELS_PATH "resources/models.txt"
#define TEMPLATE_SOURCE "resources/templates/project_template.txt"
#define EXAMPLE_TEMPLATE_PATH "resources/templates/example_template.txt"
#define TEMPLATE_OUTPUT "project_template.txt"
#define UI_TEMPLATES_DIR "resources/UI_templates_Agents"
#define ASSET_SOURCES_PATH "resources/internet-sources.txt"
#define MAX_FILE_BYTES (64L * 1024 * 1024)
#define UI_STRING_COUNT 64

// ==========================================
//      Global State
// ==========================================
char ui_strings[UI_STRING_COUNT][256];
char username[64];

// English fallbacks, used for every line a language file does not provide.
static const char* const DEFAULT_STRINGS[UI_STRING_COUNT] = {
    "Agentic Engine for Minimal Resources",
    "1. Run Agent on Existing Project",
    "2. Generate New Project Template",
    "3. Run Hardware Benchmark",
    "Choice:",
    "Enter absolute project path:",
    "Preparing benchmark. Downloading micro-model...",
    "Running tensor benchmark (Calculating Tokens/Sec)...",
    "Benchmark Complete! Processing capability:",
    "Initializing new project tree at:",
    "SUCCESS: Project initialized!",
    "Error: tasks.txt not found.",
    ">>> Next Task:",
    "Spawning Agent...",
    "Agent terminated.",
    "0. Exit",
    "Enter your name:",
    "Welcome,",
    "Goodbye,",
    "Recommended model for your hardware:",
    "Invalid option.",
    "Known projects:",
    "4. Import Filled Project Template",
    "Template generated at:",
    "A template already exists at:",
    "Fill it in, then choose the import option.",
    "Enter the path of the filled template:",
    "Template error:",
    "Project registered:",
    "Task completed:",
    "Task failed:",
    "All tasks finished.",
    "Running without supervision. Press Ctrl+C to pause.",
    "Error: Ollama is not installed. Install it with:",
    "5. Generate Project From a Description",
    "6. Settings",
    "--- Settings ---",
    "1. Change name",
    "2. Change language",
    "3. Change model",
    "0. Back",
    "Current name:",
    "Current model:",
    "Auto (recommended for this machine)",
    "Other (type a model tag)",
    "Enter the model tag:",
    "Saved.",
    "Install Ollama now? [y/n]",
    "Installing Ollama...",
    "Starting the Ollama server...",
    "Error: Ollama could not be started.",
    "STOPPED: the task kept failing. Fix the problem and run the agent again; it resumes from this task.",
    "Logs:",
    "Paused. Run the agent again to resume.",
    "Resuming from:",
    "Ollama error:",
    "Tip: choose a smaller model in Settings.",
    "Attempt",
    "Project name (letters, numbers, _ and -):",
    "Describe your project: what it does, its features and technologies. Finish with an empty line:",
    "Is it a graphical project with a user interface? [y/n]",
    "Generating the project template with the agent...",
    "Could not generate a valid template. Fix this file by hand and import it:",
    "Models [OK = fits in your RAM, X = too big] (RAM needed, estimated speed here):",
};

// ==========================================
//      Input Helpers
// ==========================================
void read_line(char* buffer, size_t size) {
    if (!fgets(buffer, size, stdin)) {
        exit(EXIT_SUCCESS);
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
}

int read_int() {
    char line[32];
    int value;
    read_line(line, sizeof(line));
    return sscanf(line, "%d", &value) == 1 ? value : -1;
}

void wait_for_enter() {
    char discard[8];
    printf("\n[Enter]");
    read_line(discard, sizeof(discard));
}

int ask_yes_no(const char* prompt) {
    char line[16];
    printf("%s ", prompt);
    read_line(line, sizeof(line));

    char first = (char)tolower((unsigned char)line[0]);
    return first == 'y' || first == 's' || first == 'o' || first == 'j' || strncmp(line, "はい", 6) == 0;
}

// ==========================================
//      Text & Path Helpers
// ==========================================
char* trim(char* text) {
    while (isspace((unsigned char)*text)) text++;
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) length--;
    text[length] = '\0';
    return text;
}

int is_valid_name(const char* name) {
    if (name[0] == '\0') return 0;
    for (const char* c = name; *c; c++) {
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-') return 0;
    }
    return 1;
}

// A relative path that cannot escape the project folder.
int is_safe_relative_path(const char* path) {
    if (path[0] == '\0' || path[0] == '/' || path[0] == '\\') return 0;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return 0;

    const char* segment = path;
    while (*segment) {
        size_t length = strcspn(segment, "/\\");
        if (length == 2 && segment[0] == '.' && segment[1] == '.') return 0;
        segment += length;
        if (*segment) segment++;
    }
    return 1;
}

void join_path(char* out, size_t size, const char* base, const char* name) {
    snprintf(out, size, "%s/%s", base, name);
}

void resolve_path(const char* path, char* resolved, size_t size) {
    char buffer[4096];
#if defined(_WIN32)
    if (_fullpath(buffer, path, sizeof(buffer))) {
#else
    if (realpath(path, buffer)) {
#endif
        snprintf(resolved, size, "%s", buffer);
    } else {
        snprintf(resolved, size, "%s", path);
    }
}

void sleep_seconds(int seconds) {
#if defined(_WIN32)
    Sleep((DWORD)seconds * 1000);
#else
    sleep((unsigned int)seconds);
#endif
}

// Total physical memory in MB, 0 when it cannot be read.
long total_ram_mb() {
#if defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? (long)(status.ullTotalPhys / (1024 * 1024)) : 0;
#elif defined(__APPLE__)
    long long bytes = 0;
    FILE *pipe = POPEN("sysctl -n hw.memsize 2>/dev/null", "r");
    if (pipe) {
        if (fscanf(pipe, "%lld", &bytes) != 1) bytes = 0;
        PCLOSE(pipe);
    }
    return (long)(bytes / (1024 * 1024));
#else
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;

    char line[256];
    long kilobytes = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %ld kB", &kilobytes) == 1) break;
    }
    fclose(f);
    return kilobytes / 1024;
#endif
}

// ==========================================
//      Display Helpers
// ==========================================
void clean_screen() {
    int status = system(CLEAN);
    (void)status;
}

void project_title() {
    clean_screen();
    printf("--- Agemir (%s) ---\n%s\n", OS_NAME, ui_strings[0]);
    printf("%s %s!\n\n", ui_strings[17], username);
}

// ==========================================
//      File Helpers
// ==========================================
int file_exists(const char* path) {
    struct stat info;
    return stat(path, &info) == 0;
}

char* read_file(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 0 || length > MAX_FILE_BYTES) {
        fclose(f);
        return NULL;
    }

    char *buffer = malloc(length + 1);
    if (buffer) {
        size_t read_bytes = fread(buffer, 1, length, f);
        buffer[read_bytes] = '\0';
    }
    fclose(f);
    return buffer;
}

int copy_file(const char* source, const char* destination) {
    FILE *in = fopen(source, "rb");
    if (!in) return 0;

    FILE *out = fopen(destination, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }

    char buffer[4096];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, count, out);
    }
    fclose(in);
    fclose(out);
    return 1;
}

void make_parent_dirs(const char* file_path) {
    char partial[1024];
    snprintf(partial, sizeof(partial), "%s", file_path);

    for (char* c = partial + 1; *c; c++) {
        if (*c == '/' || *c == '\\') {
            char separator = *c;
            *c = '\0';
            MKDIR(partial);
            *c = separator;
        }
    }
}

int write_text_file(const char* path, const char* content) {
    make_parent_dirs(path);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

// ==========================================
//      Key/Value Files
//      One "key:value" per line. The config file and every project
//      progress state use this format.
// ==========================================
int kv_get(const char* file, const char* key, char* value, size_t size) {
    FILE *f = fopen(file, "r");
    if (!f) return 0;

    char line[CONFIG_LINE];
    size_t key_len = strlen(key);
    int found = 0;

    while (!found && fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(value, size, "%s", line + key_len + 1);
            found = value[0] != '\0';
        }
    }
    fclose(f);
    return found;
}

void kv_set(const char* file, const char* key, const char* value) {
    make_parent_dirs(file);

    char temporary[1100];
    snprintf(temporary, sizeof(temporary), "%s.tmp", file);

    FILE *in = fopen(file, "r");
    FILE *out = fopen(temporary, "w");
    if (!out) {
        if (in) fclose(in);
        return;
    }

    char line[CONFIG_LINE];
    size_t key_len = strlen(key);
    if (in) {
        while (fgets(line, sizeof(line), in)) {
            int is_target = strncmp(line, key, key_len) == 0 && line[key_len] == ':';
            if (!is_target) fputs(line, out);
        }
        fclose(in);
    }
    fprintf(out, "%s:%s\n", key, value);
    fclose(out);

    remove(file);
    rename(temporary, file);
}

// ==========================================
//      Config File (resources/config/config.txt)
//      Keys: language, username, benchmark, model, project:NAME (value = PATH).
// ==========================================
int config_get(const char* key, char* value, size_t size) {
    return kv_get(CONFIG_PATH, key, value, size);
}

int config_get_int(const char* key, int fallback) {
    char value[32];
    return config_get(key, value, sizeof(value)) ? atoi(value) : fallback;
}

void config_set(const char* key, const char* value) {
    kv_set(CONFIG_PATH, key, value);
}

void config_set_int(const char* key, int value) {
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    config_set(key, text);
}

void config_register_project(const char* name, const char* path) {
    char key[128];
    snprintf(key, sizeof(key), "project:%s", name);
    config_set(key, path);
}

// Index starts at 1. Lines that are not "project:NAME:PATH" are skipped.
int config_project_at(int index, char* name, size_t name_size, char* path, size_t path_size) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return 0;

    char line[CONFIG_LINE];
    int seen = 0;
    int found = 0;

    while (!found && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "project:", 8) != 0) continue;

        line[strcspn(line, "\r\n")] = '\0';
        char* separator = strchr(line + 8, ':');
        if (!separator) continue;

        *separator = '\0';
        if (!is_valid_name(line + 8)) continue;

        if (++seen == index) {
            snprintf(name, name_size, "%s", line + 8);
            snprintf(path, path_size, "%s", separator + 1);
            found = 1;
        }
    }
    fclose(f);
    return found;
}

int config_project_count() {
    char name[64];
    char path[512];
    int count = 0;
    while (config_project_at(count + 1, name, sizeof(name), path, sizeof(path))) {
        count++;
    }
    return count;
}

// ==========================================
//      Language Loader
// ==========================================
void load_language(int lang_choice) {
    for (int i = 0; i < UI_STRING_COUNT; i++) {
        snprintf(ui_strings[i], sizeof(ui_strings[i]), "%s", DEFAULT_STRINGS[i]);
    }

    const char* filepath;
    switch (lang_choice) {
        case 1: filepath = "resources/languages/English.txt"; break;
        case 2: filepath = "resources/languages/Spanish.txt"; break;
        case 3: filepath = "resources/languages/French.txt"; break;
        case 4: filepath = "resources/languages/German.txt"; break;
        case 5: filepath = "resources/languages/Japanese.txt"; break;
        default: filepath = "resources/languages/English.txt"; break;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("Warning: Could not open %s. Using default English.\n", filepath);
        return;
    }

    char line[256];
    int i = 0;
    while (i < UI_STRING_COUNT && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0') {
            snprintf(ui_strings[i], sizeof(ui_strings[i]), "%s", line);
        }
        i++;
    }
    fclose(f);
}

#endif