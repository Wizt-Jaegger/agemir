# Agemir
Agentic Engine for Minimal Resources

## Overview
Agemir runs an autonomous coding workflow on low-end hardware. The developer fills in
one project template once; Agemir then builds the project task by task, with a local
Ollama model, without any further human intervention.

Local models need a lot of RAM and their context degrades in long chat sessions.
Agemir avoids both: every task runs in a fresh, short-lived model session and all
state lives in plain files.

## Workflow
1. Setup (once): choose a language, enter your name, and let Agemir run the hardware
   benchmark. The score selects a recommended model.
2. Generate the template (menu option 2). This copies
   resources/templates/project_template.txt to ./project_template.txt.
3. Fill it in: objective, tech stack, rules, optional style snippets, and the project
   split into GOALs, each with small TASKs (one file per task).
4. Import it (menu option 4). Agemir validates the template, builds the project tree
   in the folder you choose, and stores "project:NAME:PATH" in the config file.
5. Run the agent (menu option 1, shown only when at least one project exists).
   Agemir works through every pending task unattended.

## What happens for each task
1. A payload is built from the project context, the style snippets, the list of files
   already built, the files listed in READS, the current content of the target file
   and the task itself.
2. The payload is piped to `ollama run`. The reply must contain one fenced code block.
3. The code block is written to the task's FILE.
4. If the task has a VERIFY command it runs inside the project folder. On failure the
   error output is fed back to the model and the task is retried (3 attempts).
5. The ledger (tasks.txt) is updated ([x] done, [!] failed), a line is appended to
   progress_log.md, and the model is unloaded with `ollama stop` to free the RAM.
A task that fails all attempts leaves its file exactly as it was. Failed tasks are
retried automatically the next time the agent runs.

## Template format
PROJECT_NAME / OBJECTIVE / TECH_STACK / RULES   project description (OBJECTIVE, TECH_STACK
                                                and RULES may be repeated for more lines)
STYLE: ... END_STYLE                            optional snippets the model should imitate
GOAL:                                           groups related tasks
TASK:                                           one small, atomic job
FILE:                                           file the task writes (relative path)
READS:                                          optional, files the model must see
VERIFY:                                         optional, shell command that exits 0 when OK

## Project structure
├── Agemir.c                      menu, benchmark, startup checks
├── projectLibraries/
│   ├── utils.h                   OS macros, input, files, config, language loader
│   ├── project.h                 template parsing, import, task ledger
│   └── agent.h                   payload, model runner, verification, orchestrator loop
├── Documentation/
├── readme.txt
└── resources/
    ├── config/config.txt         generated: language, username, benchmark, projects
    ├── internet-sources.txt
    ├── languages/                English, French, German, Japanese, Spanish
    ├── templates/project_template.txt
    └── UI_templates_Agents/info.txt

Generated project folder: context.md, tasks.txt, templates/ui_guidelines.md,
progress_log.md, template.txt (copy of the imported template) and the built files.

## Execution
Compile natively using GCC or Clang:
gcc -O3 Agemir.c -o agemir

By default Ollama calls are simulated, so the workflow can be tested without Ollama.
To use the real model compile with:
gcc -O3 -DOLLAMA_ENABLED=1 Agemir.c -o agemir