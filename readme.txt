# Agemir
Agentic Engine for Minimal Resources

## Overview
Agemir runs an autonomous coding workflow on low-end hardware. You describe a project once
(by filling a template, or by describing it in plain words and letting the agent write the
template); Agemir then builds it task by task with a local Ollama model, without human help.

Local models need a lot of RAM and their context degrades in long chats. Agemir avoids both:
every task runs in a fresh, short-lived model session and all state lives in plain files.

## First run
1. Choose a language and enter your name.
2. If Ollama is not installed, Agemir offers to install it (on Linux the installer asks for sudo),
   then starts the server and waits for it.
3. The hardware benchmark runs and picks a model that fits your RAM and speed.

## Main menu
1. Run Agent on Existing Project      (only shown when at least one project exists)
2. Generate New Project Template      (copies the blank template to fill in by hand)
3. Run Hardware Benchmark
4. Import Filled Project Template
5. Generate Project From a Description
6. Settings                           (change name, language and model)
0. Exit

## Two ways to create a project
A) By hand: option 2, fill in the template, option 4 to import it.
B) By description: option 5. Give a name, a location and a description (finish with an empty
   line) and say whether it is graphical. The agent writes the template, Agemir checks it with
   the real parser (up to 5 tries, feeding the errors back) and saves it as
   <name>_gen_template.txt inside the project folder, then imports it. If it cannot produce a
   valid one, it tells you which file to fix by hand.
The project is registered in config.txt as project:NAME:PATH.

## What happens for each task
1. A payload is built: project context, style, files already built, the goal, the files in READS,
   UI templates and asset links (graphical tasks), the current file and the task itself.
2. It is piped to `ollama run`; the reply must contain one fenced code block, written to FILE.
3. VERIFY (if any) runs in the project folder. On failure the error goes back to the model.
4. Up to 5 attempts per task. Every attempt is logged in <project>/logs/<file>.attemptN.log.
5. A task that fails 5 times STOPS the whole run (later tasks depend on it). Its file is restored,
   the state is saved and you are told where the logs are. Fix the problem and run the agent
   again: it resumes from that task.
6. A fatal Ollama error (for example not enough memory) stops at once with a tip to pick a smaller
   model in Settings.
7. The model is unloaded with `ollama stop` after each task to free RAM.

## Pause and resume
- Press Ctrl+C, or create an empty file called PAUSE in the project folder. The run stops before
  the next task and the state is saved as "paused".
- Run the agent again to continue from the saved task. Progress is in progress_state.txt and the
  history in progress_log.md.

## Template format
PROJECT_NAME / OBJECTIVE / TECH_STACK / RULES   project description (OBJECTIVE, TECH_STACK and
                                                RULES may be repeated)
STYLE: ... END_STYLE                            snippets or an API CONTRACT the model should follow
GOAL:                                           groups related tasks
TASK:                                           one small job
FILE:                                           file the task writes (relative path)
RUN:                                            instead of FILE: a shell command, no model
READS:                                          optional, files the model must see
VERIFY:                                         optional, command that exits 0 when OK
UI:                                             optional, templates from UI_templates_Agents
                                                (for example UI: navbar.md,login-menu.md)
Unsafe paths (.., absolute), FILE and RUN together, and unknown UI files are rejected on import.

## Models
resources/models.txt lists the models offered in Settings (tag | active params | min RAM | auto |
note). The menu marks each as [OK] or [X] for your RAM and shows an estimated speed. "Auto" uses
the benchmark; you can pick any model or type your own tag. Sizes marked unverified in the file
should be checked with `ollama show`. Tip: Ollama's default context window is small; raise it
(OLLAMA_CONTEXT_LENGTH or num_ctx) if long tasks look truncated.

## Graphical projects
resources/UI_templates_Agents holds design guidelines (navbar, login menu, hamburger menu, cards
grid; add your own). A task with UI: gets them as a guide. resources/internet-sources.txt holds
direct links to free icons and images, one per line as name:"url", and is copied to the project.

## Terminal access
See Documentation/terminal_access.md. Short version: use RUN tasks; do not store sudo passwords.

## Project structure
├── Agemir.c                      menu, settings, benchmark, startup checks
├── projectLibraries/
│   ├── utils.h                   OS macros, input, files, config, language loader
│   ├── models.h                  model catalog, speed estimate, RAM fit
│   ├── project.h                 template parsing, import, task ledger
│   ├── agent.h                   payload, model runner, verification, orchestrator loop
│   └── generator.h               project template generator
├── Documentation/terminal_access.md
├── readme.txt
└── resources/
    ├── config/config.txt         generated: language, username, benchmark, model, projects
    ├── internet-sources.txt
    ├── languages/                English, French, German, Japanese, Spanish (64 lines each)
    ├── models.txt
    ├── templates/                project_template.txt, example_template.txt
    └── UI_templates_Agents/      info.txt and the UI guidelines

## Build
Real runs:        gcc -O3 -DOLLAMA_ENABLED=1 Agemir.c -o agemir
Simulation only:  gcc -O3 Agemir.c -o agemir   (no model is called, nothing is verified; the menu
                  shows a SIMULATION MODE banner)