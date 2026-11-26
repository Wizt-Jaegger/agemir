project('agemir', ['c', 'cpp'], version: '1.0.0',
  default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=3', 'werror=true'])

cc = meson.get_compiler('c')
gtkmm = dependency('gtkmm-4.0', version: '>=4.10')
adwaita = dependency('libadwaita-1', version: '>=1.4')
threads = dependency('threads')
gnome = import('gnome')

core = static_library('agemir-core', 'agemir_api.c',
  c_args: ['-DOLLAMA_ENABLED=1'],
  include_directories: include_directories('.'))
core_dep = declare_dependency(link_with: core, include_directories: include_directories('.'))

resources = gnome.compile_resources('agemir-resources', 'ui/agemir.gresource.xml',
  source_dir: 'ui', c_name: 'agemir', export: true)

executable('agemir', ['ui/main.cpp', 'ui/main_window.cpp', resources],
  dependencies: [core_dep, gtkmm, adwaita, threads], install: true)

# Retain a direct C CLI build for distributions and terminal-only development.
executable('agemir-cli', 'Agemir.c', c_args: ['-DOLLAMA_ENABLED=1'], install: true)

api_smoke = executable('api-smoke', 'tests/api_smoke.c',
  dependencies: core_dep)
test('api-smoke', api_smoke)
