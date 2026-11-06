#include "projectLibraries/utils.h"
#include "projectLibraries/models.h"
#include "projectLibraries/project.h"
#include "projectLibraries/agent.h"
#include "projectLibraries/generator.h"

// ==========================================
//      Hardware Benchmark
// ==========================================
#define BENCHMARK_MODEL "qwen2.5-coder:1.5b"

#if OLLAMA_ENABLED
// The only interpolated value is a sample number. Both cmd.exe's C runtime
// argument parsing and the POSIX shell pass the escaped quotes through to curl.
double benchmark_sample(int sample) {
    char command[1024];
    snprintf(command, sizeof(command),
             "curl --noproxy \"*\" --fail --silent --show-error --max-time 180 "
             "http://127.0.0.1:11434/api/generate -H \"Content-Type: application/json\" "
             "--data-binary \"{\\\"model\\\":\\\"" BENCHMARK_MODEL "\\\","
             "\\\"prompt\\\":\\\"Sample %d. List the integers 1 through 500 separated by commas.\\\","
             "\\\"stream\\\":false,\\\"keep_alive\\\":\\\"5m\\\","
             "\\\"options\\\":{\\\"temperature\\\":0,\\\"seed\\\":42,"
             "\\\"num_ctx\\\":2048,\\\"num_predict\\\":128}}\"",
             sample);
    FILE *pipe = POPEN(command, "r");
    if (!pipe) return 0.0;

    char response[16384];
    size_t length = fread(response, 1, sizeof(response) - 1, pipe);
    int extra = fgetc(pipe);
    int failed = ferror(pipe);
    int status = PCLOSE(pipe);
    if (extra != EOF || failed || status != 0) return 0.0;
    response[length] = '\0';

    const char* count_field = strstr(response, "\"eval_count\"");
    const char* duration_field = strstr(response, "\"eval_duration\"");
    if (!count_field || !duration_field) return 0.0;

    long count = 0;
    long long duration = 0;
    if (sscanf(count_field, "\"eval_count\" : %ld", &count) != 1 || count != 128 ||
        sscanf(duration_field, "\"eval_duration\" : %lld", &duration) != 1 || duration <= 0) {
        return 0.0;
    }
    return (double)count * 1e9 / (double)duration;
}
#endif

int run_hardware_benchmark() {
    double eval_rate = 24.5;
#if OLLAMA_ENABLED
    const char* host = getenv("OLLAMA_HOST");
    if (host && *host && strcmp(host, "127.0.0.1:11434") != 0 &&
        strcmp(host, "http://127.0.0.1:11434") != 0 &&
        strcmp(host, "localhost:11434") != 0 && strcmp(host, "http://localhost:11434") != 0) {
        printf("The hardware benchmark requires Ollama on 127.0.0.1:11434.\n");
        return 0;
    }
    if (!ensure_ollama_ready()) return 0;
    printf("%s %s\n", ui_strings[6], BENCHMARK_MODEL);
    if (!ensure_model(BENCHMARK_MODEL)) return 0;

    printf("%s\n", ui_strings[7]);
    system("ollama --version");
    printf("Model: %s | Context: 2048 | Output: 128 tokens per sample\n", BENCHMARK_MODEL);
    double rates[3];
    int valid = benchmark_sample(0) > 0.0;
    if (valid) system("ollama ps");
    for (int i = 0; valid && i < 3; i++) {
        rates[i] = benchmark_sample(i + 1);
        valid = rates[i] > 0.0;
        if (valid) printf("Sample %d: %.2f output tokens/sec\n", i + 1, rates[i]);
    }
    unload_model(BENCHMARK_MODEL);
    if (!valid) {
        printf("%s benchmark needs curl and a complete 128-token response for each sample.\n", ui_strings[55]);
        return 0;
    }

    // Median of three warm decode runs, excluding model load and prompt evaluation.
    if (rates[0] > rates[1]) {
        double swap = rates[0];
        rates[0] = rates[1];
        rates[1] = swap;
    }
    eval_rate = rates[2] < rates[0] ? rates[0] : rates[2] > rates[1] ? rates[1] : rates[2];
#endif

    int tokens_per_sec = (int)(eval_rate + 0.5);
    if (tokens_per_sec < 1) tokens_per_sec = 1;
    long ram_mb = total_ram_mb();
    printf("%s %.2f output tokens/sec | RAM: %ld MB\n", ui_strings[8], eval_rate, ram_mb);
    printf("%s %s\n", ui_strings[19], recommended_model(tokens_per_sec, ram_mb));

#if OLLAMA_ENABLED
    config_set_int("benchmark_v2", tokens_per_sec);
#endif
    return tokens_per_sec;
}

// ==========================================
//      Startup Checks
// ==========================================
// Returns 1-5, or 0 when the user cancels (only if allow_cancel is set).
int prompt_language(int allow_cancel) {
    int lang = -1;

    while (lang < 1 || lang > 5) {
        clean_screen();
        printf("--- Agemir (%s) ---\n", OS_NAME);
        printf("Please select a language / Seleccione un idioma / Choisissez une langue:\n\n");
        printf("\t1. English\n\t2. Español\n\t3. Français\n\t4. Deutsch\n\t5. 日本語\n");
        if (allow_cancel) printf("\t0. %s\n", ui_strings[40]);
        printf("\tSelection: ");

        lang = read_int();
        if (allow_cancel && lang == 0) return 0;
    }
    return lang;
}

int get_or_prompt_language() {
    int lang = config_get_int("language", 0);
    if (lang >= 1 && lang <= 5) {
        return lang;
    }

    lang = prompt_language(0);
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
//      Settings
// ==========================================
void change_name() {
    char name[64];
    printf("%s ", ui_strings[16]);
    read_line(name, sizeof(name));

    char* trimmed = trim(name);
    if (trimmed[0] == '\0') return;

    snprintf(username, sizeof(username), "%s", trimmed);
    config_set("username", username);
    printf("%s\n", ui_strings[46]);
}

void change_language() {
    int lang = prompt_language(1);
    if (lang == 0) return;

    config_set_int("language", lang);
    load_language(lang);
    printf("%s\n", ui_strings[46]);
}

void change_model(int benchmark_tps) {
    long ram_mb = total_ram_mb();

    printf("\n%s\n", ui_strings[63]);
    printf("\tRAM: %ld MB | benchmark: %d tokens/sec\n\n", ram_mb, benchmark_tps);
    printf("\t1. %s: %s\n", ui_strings[43], recommended_model(benchmark_tps, ram_mb));
    for (int i = 0; i < catalog_count; i++) {
        const ModelInfo* model = &catalog[i];
        printf("\t%d. [%s] %s (needs %d GB, ~%.1f tokens/sec) %s\n", i + 2,
               model_fits(model, ram_mb) ? "OK" : "X", model->tag, model->min_ram_gb,
               estimated_speed(benchmark_tps, model), model->note);
    }
    printf("\t%d. %s\n", catalog_count + 2, ui_strings[44]);
    printf("\t%s ", ui_strings[4]);

    int choice = read_int();
    if (choice == 1) {
        config_set("model", "auto");
        printf("%s\n", ui_strings[46]);
    } else if (choice >= 2 && choice < catalog_count + 2) {
        config_set("model", catalog[choice - 2].tag);
        printf("%s\n", ui_strings[46]);
    } else if (choice == catalog_count + 2) {
        char tag[64];
        printf("%s ", ui_strings[45]);
        read_line(tag, sizeof(tag));
        if (trim(tag)[0] != '\0') {
            config_set("model", trim(tag));
            printf("%s\n", ui_strings[46]);
        }
    } else {
        printf("%s\n", ui_strings[20]);
    }
}

void settings_menu(int benchmark_tps) {
    int choice = -1;

    while (choice != 0) {
        char model[64];
        int is_auto = 0;
        current_model(benchmark_tps, total_ram_mb(), model, sizeof(model), &is_auto);

        clean_screen();
        printf("%s\n\n", ui_strings[36]);
        printf("\t%s %s\n", ui_strings[41], username);
        printf("\t%s %s%s\n\n", ui_strings[42], model, is_auto ? " (auto)" : "");
        printf("\t%s\n\t%s\n\t%s\n\t%s\n", ui_strings[37], ui_strings[38], ui_strings[39], ui_strings[40]);
        printf("\t%s ", ui_strings[4]);
        choice = read_int();

        if (choice == 1) {
            change_name();
            wait_for_enter();
        } else if (choice == 2) {
            change_language();
            wait_for_enter();
        } else if (choice == 3) {
            change_model(benchmark_tps);
            wait_for_enter();
        } else if (choice != 0) {
            printf("%s\n", ui_strings[20]);
            wait_for_enter();
        }
    }
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

void print_selected_model(const char* model, int is_auto) {
    printf("%s %s\n", is_auto ? ui_strings[19] : ui_strings[42], model);
}

void handle_run_agent(const char* model, int is_auto) {
    char name[64];
    char path[512];

    if (!select_project(name, sizeof(name), path, sizeof(path))) {
        printf("%s\n", ui_strings[20]);
        return;
    }

    print_selected_model(model, is_auto);
    run_agent(path, model);
}

void handle_generate(const char* model, int is_auto, int benchmark_rate) {
    char name[64];
    char project_path[512];
    char description[MAX_DESCRIPTION_BYTES];

    printf("%s ", ui_strings[58]);
    read_line(name, sizeof(name));
    if (!is_valid_name(name)) {
        printf("%s\n", ui_strings[20]);
        return;
    }

    printf("%s ", ui_strings[5]);
    read_line(project_path, sizeof(project_path));
    if (project_path[0] == '\0') {
        printf("%s\n", ui_strings[20]);
        return;
    }

    printf("%s\n", ui_strings[59]);
    read_description(description, sizeof(description));
    if (description[0] == '\0') {
        printf("%s\n", ui_strings[20]);
        return;
    }

    int graphical = ask_yes_no(ui_strings[60]);
    if (is_auto && total_ram_mb() >= 12 * 1024 && benchmark_rate >= 10) {
        model = "qwen2.5-coder:7b";
    }
    print_selected_model(model, is_auto);
    printf("%s\n", ui_strings[61]);
    generate_project(model, name, project_path, description, graphical);
}

// ==========================================
//      Main Entry
// ==========================================
int main() {
    load_language(get_or_prompt_language());
    get_or_prompt_username();
    load_model_catalog();
    int benchmark_rate = config_get_int("benchmark_v2", -1);
    if (benchmark_rate < 0) {
        benchmark_rate = run_hardware_benchmark();
        wait_for_enter();
    }

    int choice = -1;
    while (choice != 0) {
        int has_projects = config_project_count() > 0;
        char model[64];
        int is_auto = 0;
        current_model(benchmark_rate, total_ram_mb(), model, sizeof(model), &is_auto);

        project_title();
#if !OLLAMA_ENABLED
        printf("*** SIMULATION MODE: no model is called and nothing is verified. "
               "Compile with -DOLLAMA_ENABLED=1 for real runs. ***\n\n");
#endif
        if (has_projects) printf("\t%s\n", ui_strings[1]);
        printf("\t%s\n", ui_strings[2]);
        printf("\t%s\n", ui_strings[3]);
        printf("\t%s\n", ui_strings[22]);
        printf("\t%s\n", ui_strings[34]);
        printf("\t%s\n", ui_strings[35]);
        printf("\t%s\n", ui_strings[15]);
        printf("\t%s ", ui_strings[4]);
        choice = read_int();

        if (choice == 1 && has_projects) {
            handle_run_agent(model, is_auto);
            wait_for_enter();
        } else if (choice == 2) {
            generate_template();
            wait_for_enter();
        } else if (choice == 3) {
            int rate = run_hardware_benchmark();
            if (rate > 0) benchmark_rate = rate;
            wait_for_enter();
        } else if (choice == 4) {
            import_template();
            wait_for_enter();
        } else if (choice == 5) {
            handle_generate(model, is_auto, benchmark_rate);
            wait_for_enter();
        } else if (choice == 6) {
            settings_menu(benchmark_rate);
        } else if (choice != 0) {
            printf("%s\n", ui_strings[20]);
            wait_for_enter();
        }
    }

    printf("\n%s %s!\n", ui_strings[18], username);
    return 0;
}