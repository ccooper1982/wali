project(
  'awi',
  'cpp',
  version: '0.1',
  meson_version: '>= 1.1', # for meson.options
  default_options: ['cpp_std=c++23'],
) # TODO warning_level/werror

wt_dep = dependency('wt', required: true)
# dependency() does not find wthttp lib
wthttp_dep = meson.get_compiler('cpp').find_library('wthttp', required: true)

libwali_proj = subproject('walilib')
libwali_dep = libwali_proj.get_variable('libwali_dep')

includes = include_directories(['include'])
sources = ['src/wali.cpp']

executable(
  meson.project_name(),
  sources,
  include_directories: includes,
  dependencies: [wthttp_dep, wt_dep, libwali_dep],
)
