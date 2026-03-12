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
  'saturn-threadsafe-list-store.c',
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

extra_c_args = run_command('ecl-config', '--cflags', check: true).stdout().strip().split()
extra_link_args = run_command('ecl-config', '--libs', check: true).stdout().strip().split()

static_cl_libs = [
  'split-sequence',
  'closer-mop',
  'trivial-garbage',
  'global-vars',
  'bordeaux-threads',
  'iterate',
  'babel',
  'alexandria',
  'trivial-features',
  'cffi',
  'cl-cffi-gtk4',
  'cl-cffi-gtk4-init',
  'cl-cffi-cairo',
  'cl-cffi-glib',
  'cl-cffi-glib-init',
  'cl-cffi-pango',
  'cl-cffi-graphene',
  'cl-cffi-gdk-pixbuf',
]
foreach l : static_cl_libs
  saturn_deps += [cc.find_library(l, dirs: '/app/lib/ecl-24.5.10/', static: true)]
endforeach

executable('saturn', saturn_sources,
  dependencies: saturn_deps,
       install: true,
     c_args: extra_c_args,
     link_args: extra_link_args,
)
