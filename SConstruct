# Setup
import os
import json
env = Environment(ENV = os.environ)
try:
    env.Tool('config', toolpath = [os.environ.get('CBANG_HOME')])
except Exception as e:
    raise Exception('CBANG_HOME not set?\n' + str(e))

env.CBLoadTools('compiler cbang dist build_info packager resources osx')

# Options
env.CBAddVariables(
    BoolVariable('wrap_glibc', 'Wrap some glibc functions for backwards '
                 'compatibility with older Linux OSes', 0))

conf = env.CBConfigure()

with open('package.json', 'r') as f:
  package_info = json.load(f)

# Config vars
version = package_info['version']
env.Append(BUILD_INFO_PACKAGE_VARS = 'URL')
env.Replace(RESOURCES_NS      = 'FAH::Client')
env.Replace(BUILD_INFO_NS     = 'FAH::Client::BuildInfo')
env.Replace(PACKAGE_VERSION   = version)
env.Replace(PACKAGE_AUTHOR    = package_info['author'])
env.Replace(PACKAGE_COPYRIGHT = package_info['copyright'])
env.Replace(PACKAGE_HOMEPAGE  = package_info['homepage'])
env.Replace(PACKAGE_LICENSE   = package_info['license'])
env.Replace(PACKAGE_URL       = package_info['url'])
env.Replace(PACKAGE_ORG       = 'foldingathome.org')


if not env.GetOption('clean'):
    conf.CBConfig('compiler')
    conf.CBConfig('cbang')
    env.CBDefine('USING_CBANG') # Using CBANG macro namespace

    # OSX
    if env['PLATFORM'] == 'darwin':
        conf.RequireOSXFramework('SystemConfiguration')

    # Windows Systray
    if env['PLATFORM'] == 'win32': conf.CBRequireLib('shell32')

    if env['PLATFORM'] == 'posix':
        if env['wrap_glibc']:
            funcs = 'glob logf log expf exp powf pow fcntl64'
            flags = ['-Wl,--wrap=' + x for x in funcs.split()]
            env.AppendUnique(LINKFLAGS = flags)

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
if env['PLATFORM'] == 'posix':
    distfiles.append('install/lin/fah-client.service')
if hide_console is not None: distfiles.append(hide_console)
tar = env.TarBZ2Dist('fah-client', distfiles)
Alias('dist', tar)
AlwaysBuild(tar)
Clean(tar, ['dist.txt'])

if 'package' in COMMAND_LINE_TARGETS:
    # Resolve variables in ``.in`` files and directories
    if   env['PLATFORM'] == 'win32':  paths = ['win']
    elif env['PLATFORM'] == 'darwin': paths = ['osx']
    else: paths = ['lin', 'debian', 'rpm']
    for p in paths: env.ReplaceVariablesInFiles('install/' + p, 'build/install')

    # Code sign key password
    path = os.environ.get('CODE_SIGN_KEY_PASS_FILE')
    if path is not None: code_sign_key_pass = open(path, 'r').read().strip()
    else: code_sign_key_pass = None

    if 'SIGNTOOL' in os.environ: env['SIGNTOOL'] = os.environ['SIGNTOOL']

    misc = []
    pkg_target = None
    pkg_components = []
    if env['PLATFORM'] == 'darwin':
        def seticon(info):
            """Add icon to web control url file resourse fork"""
            # brew install fileicon
            # Packager functions do not preserve rsrc fork, so use callback
            env.RunCommand(['fileicon', 'set',
                info['root'] + '/Applications/Folding@home/Folding@home.url',
                'images/fahlogo.icns'])

        # Specify components for the osx distribution pkg
        pkg_components = [dict(
            # name is component pkg file name and name shown in installer
            name =        'FAHClient',
            pkg_id =      'org.foldingathome.fahclient.pkg',
            description = 'FAH_CLIENT_DESC', # Localizable.strings key
            home =        '.', # abs path or relative to PWD
            pkg_scripts = 'build/install/osx/scripts', # relative to home
            root =        './build/pkg/root', # abs path or relative to PWD
            sign_tools =  ['usr/local/bin/fah-client'], # relative to root
            must_close_apps = [
                'org.foldingathome.fahviewer',
                'org.foldingathome.fahcontrol',
                'edu.stanford.folding.fahviewer',
                'edu.stanford.folding.fahcontrol',
            ],
            pkg_files = [
                [str(client[0]), 'usr/local/bin/', 0o755],
                ['build/install/osx/fahclient.url',
                 'Applications/Folding@home/Folding@home.url', 0o644],
                ['build/install/osx/uninstall.url',
                 'Applications/Folding@home/uninstall.url', 0o644],
                ['build/install/osx/launchd.plist',
                 'Library/LaunchDaemons/' +
                 'org.foldingathome.fahclient.plist', 0o644]
            ],
            pre_sign_callback = seticon,
        )]

        # min pkg target macos 10.13
        pkg_target = env.get('osx_min_ver', '10.13')
        ver = tuple([int(x) for x in pkg_target.split('.')])
        if ver < (10, 13): pkg_target = '10.13'

    elif env['PLATFORM'] == 'posix':
        if env.GetPackageType() == 'deb':
            polkit = [
                'build/install/lin/fah-client.pkla',
                'var/lib/polkit-1/localauthority/10-vendor.d/fah-client.pkla'
            ]
        else:
            polkit = ['build/install/lin/fah-client.rules',
                      'usr/share/polkit-1/rules.d/fah-client.rules']

        misc.append(polkit)

    # Package
    pkg = env.Packager(
        package_info.get('name'),

        version            = version,
        maintainer         = env['PACKAGE_AUTHOR'],
        vendor             = env['PACKAGE_ORG'],
        url                = env['PACKAGE_HOMEPAGE'],
        license            = 'LICENSE',
        bug_url            =
          'https://github.com/FoldingAtHome/fah-client-bastet',
        summary            = 'Folding@home Client',
        description        = package_info.get('description'),
        icons              = ['images/fahlogo.png'],

        documents          = docs,
        programs           = [str(client[0])],
        desktop_menu       = ['build/install/lin/fah-client.desktop'],
        changelog          = 'CHANGELOG.md',
        systemd            = ['build/install/lin/fah-client.service'],
        misc               = misc,

        nsi                = 'build/install/win/fah-client.nsi',
        timestamp_url      = 'http://timestamp.comodoca.com/authenticode',
        code_sign_key      = os.environ.get('CODE_SIGN_KEY', None),
        code_sign_key_pass = code_sign_key_pass,

        deb_directory      = 'build/install/debian',
        deb_section        = 'science',
        deb_depends        = 'ca-certificates',
        deb_conflicts      = 'FAHClient, fahclient',
        deb_replaces       = 'FAHClient, fahclient',
        deb_pre_depends    = 'adduser, procps',
        deb_priority       = 'optional',

        rpm_license        = env['PACKAGE_LICENSE'],
        rpm_group          = 'Applications/Internet',
        rpm_post           = 'build/install/rpm/post',
        rpm_postun         = 'build/install/rpm/postun',
        rpm_pre            = 'build/install/rpm/pre',
        rpm_preun          = 'build/install/rpm/preun',
        rpm_conflicts      = 'FAHClient, fahclient',
        rpm_obsoletes      = 'FAHClient, fahclient',
        rpm_build_requires = 'systemd-rpm-macros',
        rpm_provides       = 'user(fah-client), group(fah-client)',
        rpm_requires       = 'polkit',
        rpm_pre_requires   = 'systemd, procps, (shadow-utils or shadow)',
        rpm_post_requires  = 'systemd, coreutils',
        rpm_preun_requires = 'systemd',
        rpm_postun_requires= 'systemd',
        rpm_client_dirs    = [
            'var/lib/fah-client', 'var/log/fah-client', 'etc/fah-client'],

        pkg_type           = 'dist',
        pkg_resources      = [['build/install/osx/Resources', '.'],
                              ['LICENSE', 'en.lproj/LICENSE.txt']],
        pkg_welcome        = 'Welcome.rtf',
        pkg_readme         = 'Readme.rtf',
        pkg_license        = 'LICENSE.txt',
        pkg_conclusion     = 'Conclusion.rtf',
        pkg_background     = 'fah-opacity-50.png',
        pkg_customize      = 'ask',
        pkg_target         = pkg_target,
        pkg_components     = pkg_components,
    )

    AlwaysBuild(pkg)
    env.Alias('package', pkg)
    Clean(pkg, ['build/pkg', 'build/flatdistpkg', 'package.txt'])
