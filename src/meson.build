blueprints = custom_target('blueprints',
  input: files(
    'saturn-window.blp',
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

saturn_sources = [
  'main.c',
  'saturn-application.c',
  'saturn-window.c',
  'saturn-provider.c',
]
saturn_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.8'),
  dependency('libdex-1', version: '>= 1.0.0'),
  dependency('gtksourceview-5', version: '>= 5.17'),
  dependency('glycin-2', version: '>= 2.0'),
  dependency('glycin-gtk4-2', version: '>= 2.0'),
]
subdir('providers')

saturn_sources += gnome.compile_resources('saturn-resources',
  'saturn.gresource.xml',
  c_name: 'saturn',
  dependencies: blueprints,
)

executable('saturn', saturn_sources,
  dependencies: saturn_deps,
       install: true,
)
