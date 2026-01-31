project(
  'wali',
  'cpp',
  version: '0.9',
  meson_version: '>= 1.1', # for meson.options
  default_options: ['cpp_std=c++23'],
) # TODO warning_level/werror

### Configure wt ###
#   - everything off except SSL so we can use HTTPS requests to search Arch repo
#   - static linking
cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.add_cmake_defines(
  {
    'ENABLE_SSL': 'ON',
    'ENABLE_HARU': 'OFF',
    'ENABLED_PANGO': 'OFF',
    'ENABLE_PANGO': 'OFF',
    'ENABLE_SQLITE': 'OFF',
    'ENABLE_POSTGRES': 'OFF',
    'ENABLE_FIREBIRD': 'OFF',
    'ENABLE_MYSQL': 'OFF',
    'ENABLE_MSSQLSERVER': 'OFF',
    'ENABLE_QT4': 'OFF',
    'ENABLE_QT5': 'OFF',
    'ENABLE_QT6': 'OFF',
    'ENABLE_SAML': 'OFF',
    'ENABLE_LIBWTTEST': 'OFF',
    'ENABLE_LIBWTDBO': 'OFF',
    'ENABLE_OPENGL': 'OFF',
    'ENABLE_UNWIND': 'OFF',
    'HTTP_WITH_ZLIB': 'OFF',
    'WT_CPP17_FILESYSTEM_IMPLEMENTATION': 'std', # rather than boost
    'WT_CPP20_DATE_TZ_IMPLEMENTATION': 'std', # rather than boost
    'CONNECTOR_HTTP': 'ON',
    'CONNECTOR_FCGI': 'OFF',
    'BUILD_TESTS': 'OFF',
    'INSTALL_DOCUMENTATION': 'OFF',
    'INSTALL_THEMES': 'OFF',
    'INSTALL_RESOURCES': 'OFF',
    'SHARED_LIBS': false,
    'Boost_USE_STATIC_LIBS': true,
  },
)

opt_var.set_override_option('CMAKE_CXX_STANDARD_REQUIRED', 'ON')
opt_var.set_override_option('cpp_std', 'c++23')

wt_proj = cmake.subproject('wt', options: opt_var)

wt_dep = wt_proj.dependency('wt')
wthttp_dep = wt_proj.dependency('wthttp')

### build options ###

if (get_option('skip_validation'))
  add_project_arguments('-DWALI_SKIP_VALIDATION', language: 'cpp')
endif

if (get_option('disable_install'))
  add_project_arguments('-DWALI_DISABLE_INSTALL', language: 'cpp')
endif

if (get_option('fake_data'))
  add_project_arguments('-DWALI_FAKE_DATA', language: 'cpp')
endif

### other deps ###

# plog logging library
subproject('sergiusthebest-plog')
plog_dep = dependency('plog') #header only, but points meson to the include dir

# libmount and libmount
blkid_dep = dependency('blkid', required: true)
libmount_dep = dependency('mount', required: true)

### sources ##
includes = include_directories(['include'])
sources = [
  'src/Wali.cpp',
  'src/DiskUtils.cpp',
  'src/Install.cpp',
  'src/widgets/AccountsWidget.cpp',
  'src/widgets/InstallWidget.cpp',
  'src/widgets/LocaliseWidget.cpp',
  'src/widgets/NetworkWidget.cpp',
  'src/widgets/MountsWidget.cpp',
  'src/widgets/PackagesWidget.cpp',
  'src/widgets/PartitionsWidget.cpp',
  'src/widgets/VideoWidget.cpp',
]

executable(
  meson.project_name(),
  sources,
  include_directories: includes,
  dependencies: [wthttp_dep, wt_dep, plog_dep, blkid_dep, libmount_dep],
)
