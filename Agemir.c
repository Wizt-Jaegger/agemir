#include "projectLibraries/utils.h"
#include "projectLibraries/project.h"
#include "projectLibraries/agent.h"

// ==========================================
//      Model Recommendations
// ==========================================
typedef struct {
    int min_tokens_per_sec;
    const char* model;
} ModelTier;

static const ModelTier MODEL_TIERS[] = {
    {40, "qwen2.5-coder:14b"},
    {20, "qwen2.5-coder:7b"},
    {10, "qwen2.5-coder:3b"},
    {0,  "qwen2.5-coder:1.5b"},
};

const char* recommended_model(int tokens_per_sec) {
    size_t last = sizeof(MODEL_TIERS) / sizeof(MODEL_TIERS[0]) - 1;
    size_t i = 0;
    while (i < last && tokens_per_sec < MODEL_TIERS[i].min_tokens_per_sec) {
        i++;
    }
    return MODEL_TIERS[i].model;
}

// ==========================================
//      Hardware Benchmark
// ==========================================
int run_hardware_benchmark() {
    printf("%s\n", ui_strings[6]);

#if OLLAMA_ENABLED
    char pull_cmd[256];
    snprintf(pull_cmd, sizeof(pull_cmd), "ollama pull qwen:0.5b %s", DEV_NULL);
    int status = system(pull_cmd);
    (void)status;
#endif

    printf("%s\n", ui_strings[7]);

    float eval_rate = 0.0f;

#if OLLAMA_ENABLED
    FILE *fp = POPEN("ollama run qwen:0.5b \"Write a 50 word poem\" --verbose 2>&1", "r");
    char buffer[512];
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            char *match = strstr(buffer, "eval rate:");
            if (match) sscanf(match, "eval rate: %f", &eval_rate);
        }
        PCLOSE(fp);
    }
#else
    eval_rate = 24.5f;
#endif

    int tokens_per_sec = (int)eval_rate;
    printf("%s %d tokens/sec\n", ui_strings[8], tokens_per_sec);
    printf("%s %s\n", ui_strings[19], recommended_model(tokens_per_sec));

    config_set_int("benchmark", tokens_per_sec);
    return tokens_per_sec;
}

// ==========================================
//      Startup Checks
// ==========================================
int get_or_prompt_language() {
    int lang = config_get_int("language", 0);
    if (lang >= 1 && lang <= 5) {
        return lang;
    }

    while (lang < 1 || lang > 5) {
        clean_screen();
        printf("--- Agemir (%s) ---\n", OS_NAME);
        printf("Please select a language / Seleccione un idioma / Choisissez une langue:\n\n");
        printf("\t1. English\n\t2. Español\n\t3. Français\n\t4. Deutsch\n\t5. 日本語\n\tSelection: ");
        lang = read_int();
    }

    config_set_int("language", lang);
    return lang;
}

void get_or_prompt_username() {
    if (config_get("username", username, sizeof(username))) {
        return;
    }

    while (username[0] == '\0') {
        printf("%s ", ui_strings[16]);
        read_line(username, sizeof(username));
    }
    config_set("username", username);
}

// ==========================================
//      Menu Actions
// ==========================================
void generate_template() {
    if (file_exists(TEMPLATE_OUTPUT)) {
        printf("%s %s\n", ui_strings[24], TEMPLATE_OUTPUT);
    } else if (copy_file(TEMPLATE_SOURCE, TEMPLATE_OUTPUT)) {
        printf("%s %s\n", ui_strings[23], TEMPLATE_OUTPUT);
    } else {
        printf("%s could not read %s\n", ui_strings[27], TEMPLATE_SOURCE);
        return;
    }
    printf("%s\n", ui_strings[25]);
}

void import_template() {
    char template_path[512];
    char project_path[512];

    printf("%s ", ui_strings[26]);
    read_line(template_path, sizeof(template_path));
    printf("%s ", ui_strings[5]);
    read_line(project_path, sizeof(project_path));

    if (template_path[0] == '\0' || project_path[0] == '\0') {
        printf("%s\n", ui_strings[20]);
    } else {
        import_project(template_path, project_path);
    }
}

int select_project(char* name, size_t name_size, char* path, size_t path_size) {
    int count = config_project_count();

    printf("%s\n", ui_strings[21]);
    for (int i = 1; i <= count; i++) {
        config_project_at(i, name, name_size, path, path_size);
        printf("\t%d. %s (%s)\n", i, name, path);
    }

    printf("\t%s ", ui_strings[4]);
    return config_project_at(read_int(), name, name_size, path, path_size);
}

void handle_run_agent(int benchmark_rate) {
    char name[64];
    char path[512];

    if (!select_project(name, sizeof(name), path, sizeof(path))) {
        printf("%s\n", ui_strings[20]);
        return;
    }

    const char* model = recommended_model(benchmark_rate);
    printf("%s %s\n", ui_strings[19], model);
    run_agent(path, model);
}

// ==========================================
//      Main Entry
// ==========================================
int main() {
    load_language(get_or_prompt_language());
    get_or_prompt_username();

    int benchmark_rate = config_get_int("benchmark", -1);
    if (benchmark_rate < 0) {
        benchmark_rate = run_hardware_benchmark();
        wait_for_enter();
    }

    int choice = -1;
    while (choice != 0) {
        int has_projects = config_project_count() > 0;

        project_title();
        if (has_projects) printf("\t%s\n", ui_strings[1]);
        printf("\t%s\n", ui_strings[2]);
        printf("\t%s\n", ui_strings[3]);
        printf("\t%s\n", ui_strings[22]);
        printf("\t%s\n", ui_strings[15]);
        printf("\t%s ", ui_strings[4]);
        choice = read_int();

        if (choice == 1 && has_projects) {
            handle_run_agent(benchmark_rate);
            wait_for_enter();
        } else if (choice == 2) {
            generate_template();
            wait_for_enter();
        } else if (choice == 3) {
            benchmark_rate = run_hardware_benchmark();
            wait_for_enter();
        } else if (choice == 4) {
            import_template();
            wait_for_enter();
        } else if (choice != 0) {
            printf("%s\n", ui_strings[20]);
            wait_for_enter();
        }
    }

    printf("\n%s %s!\n", ui_strings[18], username);
    return 0;
}