# Setup
import os
import json
env = Environment(ENV = os.environ)
try:
    env.Tool('config', toolpath = [os.environ.get('CBANG_HOME')])
except Exception as e:
    raise Exception('CBANG_HOME not set?\n' + str(e))

env.CBLoadTools('compiler cbang dist build_info packager resources osx')

# Override mostly_static to default True
env.CBAddVariables(
    BoolVariable('mostly_static', 'Link most libraries statically', 1))

conf = env.CBConfigure()

with open('package.json', 'r') as f:
  package_info = json.load(f)

# Config vars
version = package_info['version']
env.Replace(RESOURCES_NS      = 'FAH::Client')
env.Replace(BUILD_INFO_NS     = 'FAH::Client::BuildInfo')
env.Replace(PACKAGE_VERSION   = version)
env.Replace(PACKAGE_AUTHOR = 'Joseph Coffland <joseph@cauldrondevelopment.com>')
env.Replace(PACKAGE_COPYRIGHT = '2022 foldingathome.org')
env.Replace(PACKAGE_HOMEPAGE  = 'https://foldingathome.org/')
env.Replace(PACKAGE_LICENSE   = 'https://www.gnu.org/licenses/gpl-3.0.txt')
env.Replace(PACKAGE_ORG       = 'foldingathome.org')


if not env.GetOption('clean'):
    conf.CBConfig('compiler')
    conf.CBConfig('cbang')
    env.CBDefine('USING_CBANG') # Using CBANG macro namespace

    # OSX
    if env['PLATFORM'] == 'darwin':
        if not conf.CheckOSXFramework('SystemConfiguration'):
            raise Exception('Need SystemConfiguration framework')

    # Windows Systray
    if env['PLATFORM'] == 'win32': conf.CBRequireLib('shell32')

    if env['PLATFORM'] == 'posix':
        env.Append(PREFER_DYNAMIC = 'bz2 z m'.split())

conf.Finish()

# Client
Export('env')
client = SConscript('src/client.scons', variant_dir = 'build', duplicate = 0)
Default(client)

# HideConsole
hide_console = None
if env['PLATFORM'] == 'win32' or int(env.get('cross_mingw', 0)):
    hide_console = SConscript('src/HideConsole.scons', variant_dir = 'build',
                              duplicate = 0)
    Default(hide_console)

# Clean
Clean(client, ['build', 'config.log'])

# Dist
docs = ['README.md', 'CHANGELOG.md', 'LICENSE']
distfiles = docs + [client, 'images/fahlogo.png']
if hide_console is not None: distfiles.append(hide_console)
tar = env.TarBZ2Dist('fah-client', distfiles)
Alias('dist', tar)
AlwaysBuild(tar)

description = \
'''Folding@home performs research on human diseases using the computing
resources of volunteers.
'''

short_description = package_info.get('description', 'Folding@home client')

description += short_description

if 'package' in COMMAND_LINE_TARGETS:
    # Code sign key password
    path = os.environ.get('CODE_SIGN_KEY_PASS_FILE')
    if path is not None:
        code_sign_key_pass = open(path, 'r').read().strip()
    else: code_sign_key_pass = None

    if 'SIGNTOOL' in os.environ: env['SIGNTOOL'] = os.environ['SIGNTOOL']

    distpkg_target = None
    distpkg_components = []
    if env['PLATFORM'] == 'darwin':
        # Specify components for the osx distribution pkg
        client_home = '.'
        client_root = client_home + '/build/pkg/root'
        pkg_files = [[str(client[0]), 'usr/local/bin/', 0o755],
                 ['install/osx/fahclient.url',
                  'Applications/Folding@home/fahclient.url', 0o644],
                 ['install/osx/uninstall.url',
                  'Applications/Folding@home/uninstall.url', 0o644],
                 ['install/osx/launchd.plist', 'Library/LaunchDaemons/' +
                  'org.foldingathome.fahclient.plist', 0o644]]
        distpkg_components = [
            {
                # name is component pkg file name and name shown in installer
                'name'        : 'FAHClient',
                'pkg_id'      : 'org.foldingathome.fahclient.pkg',
                'description' : short_description,
                # abs path or relative to PWD
                # client repo directory
                'home'        : client_home,
                # relative to home
                'pkg_scripts' : 'install/osx/scripts',
                # abs path or relative to PWD
                # default build/pkg/root, as per cbang config pkg module
                'root'        : client_root,
                # relative to root
                'sign_tools'  : ['usr/local/bin/fah-client'],
                'must_close_apps': [
                    'org.foldingathome.fahviewer',
                    'org.foldingathome.fahcontrol',
                    'edu.stanford.folding.fahviewer',
                    'edu.stanford.folding.fahcontrol',
                    ],
                'pkg_files'   : pkg_files,
            },
        ]

        # min pkg target macos 10.13
        distpkg_target = env.get('osx_min_ver', '10.13')
        ver = tuple([int(x) for x in distpkg_target.split('.')])
        if ver < (10,13):
            distpkg_target = '10.13'

    # Package
    pkg = env.Packager(
        package_info.get('name', 'fah-client'),

        version            = version,
        maintainer         = env['PACKAGE_AUTHOR'],
        vendor             = env['PACKAGE_ORG'],
        url                = env['PACKAGE_HOMEPAGE'],
        license            = 'copyright',
        bug_url            = 'https://github.com/FoldingAtHome/fah-client',
        summary            = 'Folding@home Client ' + version,
        description        = description,
        short_description  = short_description,
        prefix             = '/usr',
        icons              = ('images/fahlogo.icns', 'images/fahlogo.png'),

        documents          = docs,
        programs           = [str(client[0])],
        desktop_menu       = ['install/lin/fah-client.desktop'],
        changelog          = 'CHANGELOG.md',
        platform_independent = ['install/lin/fah-client.service'],

        nsi                = 'install/win/fah-client.nsi',
        nsi_dll_deps       = [str(client[0])],
        timestamp_url      = 'http://timestamp.comodoca.com/authenticode',
        code_sign_key      = os.environ.get('CODE_SIGN_KEY', None),
        code_sign_key_pass = code_sign_key_pass,

        deb_directory      = 'install/debian',
        deb_section        = 'science',
        deb_depends        = 'libc6',
        deb_conflicts      = 'FAHClient',
        deb_pre_depends    = 'adduser',
        deb_priority       = 'optional',

        rpm_license        = 'Restricted',
        rpm_group          = 'Applications/Internet',
        rpm_pre_requires   = 'shadow-utils',
        rpm_requires       = 'expat, bzip2-libs',
        rpm_build          = 'install/rpm/build',
        rpm_post           = 'install/rpm/post',
        rpm_preun          = 'install/rpm/preun',

        pkg_type           = 'dist',
        distpkg_resources  = [['install/osx/Resources', '.']],
        distpkg_welcome    = 'Welcome.rtf',
        distpkg_license    = 'License.rtf',
        distpkg_background = 'fah-opacity-50.png',
        distpkg_customize  = 'always',
        distpkg_target     = distpkg_target,
        distpkg_arch       = env.get('package_arch', 'x86_64'),
        distpkg_components = distpkg_components,
    )

    AlwaysBuild(pkg)
    env.Alias('package', pkg)
    Clean(pkg, ['build/pkg', 'build/flatdistpkg', 'package-description.txt'])
    NoClean(pkg, [Glob('*.zip'), 'package.txt'])

    with open('package-description.txt', 'w') as f:
        f.write(short_description.strip())
