<div align="center">

# 🤖 Agemir

### **A**gentic **E**ngine for **M**inimal **R**esources

*Build whole software projects with a local AI model, even on low-end hardware.*
*Construye proyectos completos con un modelo de IA local, incluso en equipos modestos.*

![Language: C](https://img.shields.io/badge/language-C-blue?style=for-the-badge&logo=c)
![Powered by Ollama](https://img.shields.io/badge/powered%20by-Ollama-black?style=for-the-badge)
![Runs locally](https://img.shields.io/badge/runs-100%25%20local-success?style=for-the-badge)
![Languages](https://img.shields.io/badge/UI-EN%20%7C%20ES%20%7C%20FR%20%7C%20DE%20%7C%20JA-orange?style=for-the-badge)

**[🇬🇧 English](#-english)** · **[🇪🇸 Español](#-español)**

</div>

---

# 🇬🇧 English

## 📑 Table of contents

- [What is Agemir?](#-what-is-agemir)
- [Why Agemir?](#-why-agemir)
- [Quick start](#-quick-start)
- [Main menu](#-main-menu)
- [Two ways to create a project](#-two-ways-to-create-a-project)
- [How each task works](#-how-each-task-works)
- [Pause and resume](#-pause-and-resume)
- [Template format](#-template-format)
- [Models](#-models)
- [Graphical projects](#-graphical-projects)
- [Terminal access](#-terminal-access)
- [Project structure](#-project-structure)
- [Build](#-build)

## ✨ What is Agemir?

Agemir runs an **autonomous coding workflow** on low-end hardware. You describe a project **once**, either by filling in a template or by describing it in plain words and letting the agent write the template. Agemir then builds it **task by task** with a local [Ollama](https://ollama.com) model, **without human help**.

## 💡 Why Agemir?

Local models need a lot of RAM, and their context degrades in long chats. Agemir avoids both problems:

| Problem | Agemir's solution |
|---|---|
| 🧠 Context degrades in long chats | Every task runs in a **fresh, short-lived** model session |
| 💾 High RAM usage | The model is **unloaded after each task** (`ollama stop`) |
| 📂 Lost progress | All state lives in **plain files** you can read and edit |

## 🚀 Quick start

1. **Build it** (see [Build](#-build)).
2. **Run it** and follow the first-run steps:
   1. Choose a language and enter your name.
   2. If Ollama is not installed, Agemir offers to install it, then starts the server and waits for it.
   3. A hardware benchmark runs and picks a model that fits your RAM and speed.
3. Create a project from the [main menu](#-main-menu).

> [!NOTE]
> On Linux, the Ollama installer asks for `sudo`.

## 🧭 Main menu

| # | Option | Description |
|:-:|---|---|
| 1 | **Run Agent on Existing Project** | Only shown when at least one project exists |
| 2 | **Generate New Project Template** | Copies the blank template for you to fill in by hand |
| 3 | **Run Hardware Benchmark** | Re-tests your machine |
| 4 | **Import Filled Project Template** | Registers a template you filled in |
| 5 | **Generate Project From a Description** | The agent writes the template for you |
| 6 | **Settings** | Change name, language and model |
| 0 | **Exit** | |

## 🛠️ Two ways to create a project

### A) By hand ✍️

Option **2** → fill in the template → option **4** to import it.

### B) By description 💬

Option **5**. Give a **name**, a **location** and a **description** (finish with an empty line), and say whether the project is **graphical**.

1. The agent writes the template.
2. Agemir checks it with the **real parser** (up to **5 tries**, feeding the errors back to the model).
3. It is saved as `<name>_gen_template.txt` inside the project folder, then imported.

> [!TIP]
> If the agent cannot produce a valid template, Agemir tells you **which file to fix by hand**.

The project is registered in `config.txt` as `project:NAME:PATH`.

## ⚙️ How each task works

1. 📦 **A payload is built:** project context, style, files already built, the goal, the files in `READS`, UI templates and asset links (graphical tasks), the current file and the task itself.
2. 🤖 **It is piped to `ollama run`.** The reply must contain **one fenced code block**, which is written to `FILE`.
3. ✅ **`VERIFY`** (if any) runs in the project folder. On failure, the error goes back to the model.
4. 🔁 **Up to 5 attempts per task.** Every attempt is logged in `<project>/logs/<file>.attemptN.log`.
5. 🛑 **A task that fails 5 times stops the whole run**, because later tasks depend on it. Its file is restored, the state is saved and you are told where the logs are. Fix the problem and run the agent again: it **resumes from that task**.
6. ⚠️ **A fatal Ollama error** (for example, not enough memory) stops at once with a tip to pick a smaller model in Settings.
7. 🧹 **The model is unloaded** with `ollama stop` after each task to free RAM.

## ⏸️ Pause and resume

- Press **Ctrl+C**, or create an **empty file called `PAUSE`** in the project folder.
- The run stops before the next task and the state is saved as `paused`.
- Run the agent again to continue from the saved task.

| File | Purpose |
|---|---|
| `progress_state.txt` | Current progress |
| `progress_log.md` | Full history |

## 📝 Template format

| Keyword | Meaning |
|---|---|
| `PROJECT_NAME` / `OBJECTIVE` / `TECH_STACK` / `RULES` | Project description (`OBJECTIVE`, `TECH_STACK` and `RULES` may be repeated) |
| `STYLE: ... END_STYLE` | Snippets or an API contract the model should follow |
| `GOAL:` | Groups related tasks |
| `TASK:` | One small job |
| `FILE:` | File the task writes (relative path) |
| `RUN:` | Used **instead of** `FILE:`: a shell command, no model involved |
| `READS:` | *Optional.* Files the model must see |
| `VERIFY:` | *Optional.* Command that exits with `0` when everything is OK |
| `UI:` | *Optional.* Templates from `UI_templates_Agents` (e.g. `UI: navbar.md,login-menu.md`) |

> [!WARNING]
> On import, Agemir **rejects**: unsafe paths (`..` or absolute paths), `FILE` and `RUN` used together, and unknown `UI` files.

## 🧠 Models

`resources/models.txt` lists the models offered in Settings:

```text
tag | active params | min RAM | auto | note
```

- The menu marks each model as **[OK]** or **[X]** for your RAM and shows an **estimated speed**.
- **"Auto"** uses the benchmark result. You can also pick any model or type your own tag.
- Auto workers range from Qwen2.5 Coder 0.5B to 3B. For **Generate Project From a Description**, Auto uses the 7B planner when the machine reports at least 12 GiB of RAM and a measured benchmark of at least 10 output tokens/s. A manual model selection applies to both planning and coding.
- The generated template keeps the requested outcomes and shared interfaces in `OBJECTIVE` and `STYLE` (elephant memory). Each coding request sees one task, its direct `READS`, and its acceptance check (goldfish scope). The generated plan ends with an integration check.

### Compare two machines

Run menu option **3** on each machine with Ollama listening on the default local port and `curl` on PATH. Compare the median of three warm 128-token runs using `qwen2.5-coder:1.5b`, a 2048-token context and fixed sampling options. Save the Ollama version and `ollama ps` row, including model ID and the `PROCESSOR` column, with each result. The throughput measures output generation. Load and prompt evaluation have separate timings. Settings estimates for other model sizes are approximate.

On Windows, Agemir checks the standard per-user Ollama directory when the current session has a stale PATH. For a custom install directory, add it to PATH and launch Agemir from a fresh terminal.

> [!TIP]
> Ollama's default context window is small. If long tasks look truncated, raise it with `OLLAMA_CONTEXT_LENGTH` or `num_ctx`.

## 🎨 Graphical projects

- `resources/UI_templates_Agents` holds **design guidelines**: navbar, login menu, hamburger menu, cards grid (add your own). A task with `UI:` receives them as a guide.
- `resources/internet-sources.txt` holds **direct links to free icons and images**, one per line as `name:"url"`, and is copied into the project.

## 💻 Terminal access

See [`Documentation/terminal_access.md`](Documentation/terminal_access.md).

**Short version:** use `RUN` tasks and **do not store sudo passwords**.

## 🗂️ Project structure

```text
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
```

## 🔨 Build

**Real runs** (calls the model):

```bash
gcc -O3 -DOLLAMA_ENABLED=1 Agemir.c -o agemir
```

**Simulation only** (no model is called, nothing is verified; the menu shows a *SIMULATION MODE* banner):

```bash
gcc -O3 Agemir.c -o agemir
```

<div align="right">

[⬆ Back to top](#-agemir)

</div>

---

# 🇪🇸 Español

## 📑 Tabla de contenidos

- [¿Qué es Agemir?](#-qué-es-agemir)
- [¿Por qué Agemir?](#-por-qué-agemir)
- [Inicio rápido](#-inicio-rápido)
- [Menú principal](#-menú-principal)
- [Dos formas de crear un proyecto](#-dos-formas-de-crear-un-proyecto)
- [Cómo funciona cada tarea](#-cómo-funciona-cada-tarea)
- [Pausar y reanudar](#-pausar-y-reanudar)
- [Formato de la plantilla](#-formato-de-la-plantilla)
- [Modelos](#-modelos)
- [Proyectos gráficos](#-proyectos-gráficos)
- [Acceso a la terminal](#-acceso-a-la-terminal)
- [Estructura del proyecto](#-estructura-del-proyecto)
- [Compilación](#-compilación)

## ✨ ¿Qué es Agemir?

Agemir ejecuta un **flujo de programación autónomo** en equipos de bajos recursos. Describes un proyecto **una sola vez**, ya sea llenando una plantilla o describiéndolo con tus propias palabras para que el agente escriba la plantilla. Después, Agemir lo construye **tarea por tarea** con un modelo local de [Ollama](https://ollama.com), **sin ayuda humana**.

## 💡 ¿Por qué Agemir?

Los modelos locales necesitan mucha RAM y su contexto se degrada en conversaciones largas. Agemir evita ambos problemas:

| Problema | Solución de Agemir |
|---|---|
| 🧠 El contexto se degrada en chats largos | Cada tarea se ejecuta en una **sesión nueva y de corta duración** |
| 💾 Alto uso de RAM | El modelo se **descarga tras cada tarea** (`ollama stop`) |
| 📂 Pérdida de progreso | Todo el estado vive en **archivos de texto** que puedes leer y editar |

## 🚀 Inicio rápido

1. **Compílalo** (consulta [Compilación](#-compilación)).
2. **Ejecútalo** y sigue los pasos de la primera ejecución:
   1. Elige un idioma e ingresa tu nombre.
   2. Si Ollama no está instalado, Agemir ofrece instalarlo, luego inicia el servidor y espera a que esté listo.
   3. Se ejecuta una prueba de rendimiento del equipo y se elige un modelo adecuado para tu RAM y velocidad.
3. Crea un proyecto desde el [menú principal](#-menú-principal).

> [!NOTE]
> En Linux, el instalador de Ollama solicita `sudo`.

## 🧭 Menú principal

| # | Opción | Descripción |
|:-:|---|---|
| 1 | **Ejecutar el agente en un proyecto existente** | Solo aparece si existe al menos un proyecto |
| 2 | **Generar nueva plantilla de proyecto** | Copia la plantilla en blanco para llenarla a mano |
| 3 | **Ejecutar prueba de rendimiento** | Vuelve a evaluar tu equipo |
| 4 | **Importar plantilla de proyecto llena** | Registra una plantilla que ya llenaste |
| 5 | **Generar proyecto a partir de una descripción** | El agente escribe la plantilla por ti |
| 6 | **Configuración** | Cambia nombre, idioma y modelo |
| 0 | **Salir** | |

## 🛠️ Dos formas de crear un proyecto

### A) A mano ✍️

Opción **2** → llena la plantilla → opción **4** para importarla.

### B) Por descripción 💬

Opción **5**. Indica un **nombre**, una **ubicación** y una **descripción** (termina con una línea vacía), y di si el proyecto es **gráfico**.

1. El agente escribe la plantilla.
2. Agemir la revisa con el **analizador real** (hasta **5 intentos**, devolviendo los errores al modelo).
3. Se guarda como `<nombre>_gen_template.txt` dentro de la carpeta del proyecto y luego se importa.

> [!TIP]
> Si el agente no logra producir una plantilla válida, Agemir te indica **qué archivo corregir a mano**.

El proyecto se registra en `config.txt` como `project:NOMBRE:RUTA`.

## ⚙️ Cómo funciona cada tarea

1. 📦 **Se construye un paquete de contexto:** contexto del proyecto, estilo, archivos ya creados, el objetivo (goal), los archivos de `READS`, plantillas de interfaz y enlaces de recursos (tareas gráficas), el archivo actual y la tarea en sí.
2. 🤖 **Se envía a `ollama run`.** La respuesta debe contener **un bloque de código**, que se escribe en `FILE`.
3. ✅ **`VERIFY`** (si existe) se ejecuta en la carpeta del proyecto. Si falla, el error se devuelve al modelo.
4. 🔁 **Hasta 5 intentos por tarea.** Cada intento se registra en `<proyecto>/logs/<archivo>.attemptN.log`.
5. 🛑 **Si una tarea falla 5 veces, se detiene toda la ejecución**, porque las tareas siguientes dependen de ella. Su archivo se restaura, el estado se guarda y se te indica dónde están los registros. Corrige el problema y vuelve a ejecutar el agente: **continúa desde esa tarea**.
6. ⚠️ **Un error grave de Ollama** (por ejemplo, memoria insuficiente) detiene todo de inmediato, con la sugerencia de elegir un modelo más pequeño en Configuración.
7. 🧹 **El modelo se descarga** con `ollama stop` después de cada tarea para liberar RAM.

## ⏸️ Pausar y reanudar

- Presiona **Ctrl+C**, o crea un **archivo vacío llamado `PAUSE`** en la carpeta del proyecto.
- La ejecución se detiene antes de la siguiente tarea y el estado se guarda como `paused`.
- Vuelve a ejecutar el agente para continuar desde la tarea guardada.

| Archivo | Propósito |
|---|---|
| `progress_state.txt` | Progreso actual |
| `progress_log.md` | Historial completo |

## 📝 Formato de la plantilla

| Palabra clave | Significado |
|---|---|
| `PROJECT_NAME` / `OBJECTIVE` / `TECH_STACK` / `RULES` | Descripción del proyecto (`OBJECTIVE`, `TECH_STACK` y `RULES` pueden repetirse) |
| `STYLE: ... END_STYLE` | Fragmentos o un contrato de API que el modelo debe seguir |
| `GOAL:` | Agrupa tareas relacionadas |
| `TASK:` | Un trabajo pequeño |
| `FILE:` | Archivo que escribe la tarea (ruta relativa) |
| `RUN:` | Se usa **en lugar de** `FILE:`: un comando de shell, sin modelo |
| `READS:` | *Opcional.* Archivos que el modelo debe ver |
| `VERIFY:` | *Opcional.* Comando que termina con `0` cuando todo está bien |
| `UI:` | *Opcional.* Plantillas de `UI_templates_Agents` (p. ej. `UI: navbar.md,login-menu.md`) |

> [!WARNING]
> Al importar, Agemir **rechaza**: rutas inseguras (`..` o rutas absolutas), `FILE` y `RUN` usados juntos, y archivos `UI` desconocidos.

## 🧠 Modelos

`resources/models.txt` enumera los modelos disponibles en Configuración:

```text
etiqueta | parámetros activos | RAM mínima | auto | nota
```

- El menú marca cada modelo como **[OK]** o **[X]** según tu RAM y muestra una **velocidad estimada**.
- **"Auto"** usa el resultado de la prueba de rendimiento. También puedes elegir cualquier modelo o escribir tu propia etiqueta.
- El modo automático usa Qwen2.5 Coder de 0.5B a 3B para programar. Al generar un proyecto desde una descripción, usa el planificador de 7B si hay al menos 12 GiB de RAM y la prueba alcanza 10 tokens de salida por segundo. La selección manual se aplica a ambas etapas.
- La plantilla conserva los resultados y contratos en `OBJECTIVE` y `STYLE`. Cada tarea incluye solo sus dependencias directas en `READS` y su verificación. El plan generado termina con una comprobación de integración.

### Comparar dos equipos

Ejecuta la opción **3** en cada equipo con Ollama en el puerto local predeterminado y `curl` en PATH. La prueba informa la mediana de tres ejecuciones de 128 tokens tras calentar `qwen2.5-coder:1.5b`, con un contexto de 2048 tokens. Guarda la versión de Ollama y la fila de `ollama ps`, incluido el ID del modelo y la columna `PROCESSOR`, junto a cada resultado. La cifra mide la generación de tokens de salida. Las velocidades de otros tamaños de modelo son estimaciones.

En Windows, Agemir busca Ollama en la carpeta estándar del usuario si la sesión aún tiene el PATH anterior. Si elegiste una carpeta de instalación personalizada, agrégala a PATH y abre Agemir desde una terminal nueva.

> [!TIP]
> La ventana de contexto predeterminada de Ollama es pequeña. Si las tareas largas se ven cortadas, auméntala con `OLLAMA_CONTEXT_LENGTH` o `num_ctx`.

## 🎨 Proyectos gráficos

- `resources/UI_templates_Agents` contiene **guías de diseño**: barra de navegación, menú de inicio de sesión, menú hamburguesa y cuadrícula de tarjetas (puedes agregar las tuyas). Una tarea con `UI:` las recibe como guía.
- `resources/internet-sources.txt` contiene **enlaces directos a íconos e imágenes gratuitas**, uno por línea con el formato `nombre:"url"`, y se copia al proyecto.

## 💻 Acceso a la terminal

Consulta [`Documentation/terminal_access.md`](Documentation/terminal_access.md).

**Versión corta:** usa tareas `RUN` y **no guardes contraseñas de sudo**.

## 🗂️ Estructura del proyecto

```text
├── Agemir.c                      menú, configuración, benchmark, verificaciones de inicio
├── projectLibraries/
│   ├── utils.h                   macros de SO, entrada, archivos, config, cargador de idiomas
│   ├── models.h                  catálogo de modelos, estimación de velocidad, ajuste de RAM
│   ├── project.h                 análisis de plantillas, importación, registro de tareas
│   ├── agent.h                   payload, ejecutor del modelo, verificación, bucle orquestador
│   └── generator.h               generador de plantillas de proyecto
├── Documentation/terminal_access.md
├── readme.txt
└── resources/
    ├── config/config.txt         generado: idioma, usuario, benchmark, modelo, proyectos
    ├── internet-sources.txt
    ├── languages/                inglés, francés, alemán, japonés, español (64 líneas c/u)
    ├── models.txt
    ├── templates/                project_template.txt, example_template.txt
    └── UI_templates_Agents/      info.txt y las guías de interfaz
```

## 🔨 Compilación

**Ejecuciones reales** (llama al modelo):

```bash
gcc -O3 -DOLLAMA_ENABLED=1 Agemir.c -o agemir
```

**Solo simulación** (no se llama a ningún modelo ni se verifica nada; el menú muestra un aviso de *MODO SIMULACIÓN*):

```bash
gcc -O3 Agemir.c -o agemir
```

<div align="right">

[⬆ Volver arriba](#-agemir)

</div>
