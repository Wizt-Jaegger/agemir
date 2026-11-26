#ifndef AGEMIR_API_H
#define AGEMIR_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Legacy CLI builds compile Agemir.c. C++ application builds compile
 * agemir_api.c as the sole C core translation unit; do not link both C files
 * into one executable because the core is currently defined in included C
 * modules.
 */
typedef struct AgemirRuntime AgemirRuntime;

typedef enum AgemirOperation {
    AGEMIR_RUN_PROJECT = 1,
    AGEMIR_GENERATE_PROJECT = 2
} AgemirOperation;

typedef struct AgemirRequest {
    AgemirOperation operation;
    const char *project_path;
    const char *project_name;
    const char *description;
    const char *model;
    int graphical;
} AgemirRequest;

typedef struct AgemirStatus {
    char state[32];
    char task[512];
    char reason[200];
    int attempts;
    char updated[32];
} AgemirStatus;

typedef struct AgemirProject {
    char name[64];
    char path[512];
} AgemirProject;

/*
 * This C ABI has no GTK/C++ types and allocates no caller-owned result buffers.
 * It fronts legacy process-global state, so create one runtime and allow only
 * one active execute call in the process. Runtime/settings calls are not
 * generally thread-safe. Destroy only after execute has returned.
 * Runtime initialization never prompts or performs network/model work.
 */
AgemirRuntime *agemir_runtime_create(void);
void agemir_runtime_destroy(AgemirRuntime *runtime);

/*
 * Settings keys are username, model, language, and benchmark_v2 for get;
 * set accepts username, model, and language. String results are copied into
 * the caller's buffer. A too-small output buffer fails without allocating.
 */
int agemir_runtime_get_setting(AgemirRuntime *runtime, const char *key,
                               char *value, size_t value_size);
int agemir_runtime_set_setting(AgemirRuntime *runtime, const char *key,
                               const char *value);
/* Project indices are zero-based; returned fields are copied into the struct. */
int agemir_runtime_project_count(AgemirRuntime *runtime);
int agemir_runtime_project_at(AgemirRuntime *runtime, int index,
                              AgemirProject *project);

/*
 * execute is synchronous; each request string must be NUL-terminated and stay
 * valid only until return. The optional error buffer is caller-owned, receives
 * NUL-terminated text on operation failure, and is never allocated or retained
 * by the runtime. It can be NULL/zero-sized to discard error text.
 * Legacy task failures and pauses are reflected by get_status() and also
 * return 0.
 * There are no callbacks, event subscriptions, or internal worker threads in
 * this slice. Callers that need a responsive UI may call execute on their own
 * worker and poll copied get_status() values; future notifications must remain
 * value-based and must not call a UI toolkit from the C core. Core diagnostics
 * may still be printed to stdout/stderr.
 * While execute runs, get_status() and pause() may be used for that project;
 * settings/project-list calls must be serialized outside the active job.
 */
int agemir_runtime_execute(AgemirRuntime *runtime, const AgemirRequest *request,
                           char *error, size_t error_size);
/*
 * Copies a caller-owned, best-effort status snapshot. It is not an atomic
 * multi-field transaction while the core is updating progress. This function
 * and pause() are the only API calls intended to overlap execute().
 */
int agemir_runtime_get_status(AgemirRuntime *runtime, const char *project_path,
                              AgemirStatus *status);
int agemir_runtime_pause(AgemirRuntime *runtime, const char *project_path,
                         char *error, size_t error_size);

/* Existing interactive command-line experience. */
int agemir_cli_main(void);

#ifdef __cplusplus
}
#endif
#endif
