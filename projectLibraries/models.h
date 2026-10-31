#ifndef MODELS_H
#define MODELS_H

#include "utils.h"

#define MAX_MODELS 32
#define BENCHMARK_MODEL_PARAMS 0.5f
#define MIN_ESTIMATED_TPS 2.0f
#define DEFAULT_BENCHMARK_TPS 20

// One line of resources/models.txt:
//   tag | active parameters (billions) | min RAM (GB) | auto (1/0) | note
typedef struct {
    char tag[64];
    float active_params_b;
    int min_ram_gb;
    int automatic;
    char note[128];
} ModelInfo;

ModelInfo catalog[MAX_MODELS];
int catalog_count = 0;

void load_model_catalog() {
    catalog_count = 0;

    FILE *f = fopen(MODELS_PATH, "r");
    if (f) {
        char line[512];
        while (catalog_count < MAX_MODELS && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char* text = trim(line);
            if (text[0] == '\0' || text[0] == '#') continue;

            char* fields[5];
            int count = 0;
            char* cursor = text;
            fields[count++] = cursor;
            while (count < 5 && (cursor = strchr(cursor, '|')) != NULL) {
                *cursor++ = '\0';
                fields[count++] = cursor;
            }
            if (count < 5) continue;

            ModelInfo* model = &catalog[catalog_count];
            snprintf(model->tag, sizeof(model->tag), "%s", trim(fields[0]));
            model->active_params_b = (float)atof(fields[1]);
            model->min_ram_gb = atoi(fields[2]);
            model->automatic = atoi(fields[3]);
            snprintf(model->note, sizeof(model->note), "%s", trim(fields[4]));
            if (model->tag[0] != '\0' && model->active_params_b > 0.0f) catalog_count++;
        }
        fclose(f);
    }

    if (catalog_count == 0) {
        ModelInfo* model = &catalog[catalog_count++];
        snprintf(model->tag, sizeof(model->tag), "qwen2.5-coder:1.5b");
        model->active_params_b = 1.5f;
        model->min_ram_gb = 3;
        model->automatic = 1;
        snprintf(model->note, sizeof(model->note), "Fallback model (resources/models.txt not found)");
    }
}

// Speed scales roughly with 1 / active parameters, so the benchmark of the
// 0.5b model tells us how fast a bigger one would be.
float estimated_speed(int benchmark_tps, const ModelInfo* model) {
    if (benchmark_tps <= 0) benchmark_tps = DEFAULT_BENCHMARK_TPS;
    return (float)benchmark_tps * BENCHMARK_MODEL_PARAMS / model->active_params_b;
}

// Unknown RAM counts as "fits". 10% tolerance because a "16 GB" PC reports about 15.5 GB.
int model_fits(const ModelInfo* model, long ram_mb) {
    return ram_mb <= 0 || ram_mb * 10 >= (long)model->min_ram_gb * 1024 * 9;
}

// First automatic model (the list is ordered best first) that fits the RAM and is fast enough.
const char* recommended_model(int benchmark_tps, long ram_mb) {
    const ModelInfo* smallest = NULL;

    for (int i = 0; i < catalog_count; i++) {
        const ModelInfo* model = &catalog[i];
        if (!model->automatic) continue;

        if (!smallest || model->min_ram_gb < smallest->min_ram_gb) smallest = model;
        if (model_fits(model, ram_mb) && estimated_speed(benchmark_tps, model) >= MIN_ESTIMATED_TPS) {
            return model->tag;
        }
    }
    return smallest ? smallest->tag : catalog[0].tag;
}

// The model the user chose in Settings, or the recommended one while it is set to auto.
void current_model(int benchmark_tps, long ram_mb, char* tag, size_t size, int* is_auto) {
    char saved[64];
    if (config_get("model", saved, sizeof(saved)) && strcmp(saved, "auto") != 0) {
        snprintf(tag, size, "%s", saved);
        *is_auto = 0;
    } else {
        snprintf(tag, size, "%s", recommended_model(benchmark_tps, ram_mb));
        *is_auto = 1;
    }
}

#endif