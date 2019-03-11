# Setup
import os
env = Environment(ENV = os.environ)
try:
    env.Tool('config', toolpath = [os.environ.get('CBANG_HOME')])
except Exception, e:
    raise Exception, 'CBANG_HOME not set?\n' + str(e)

env.CBLoadTools('compiler cbang dist build_info packager resources osx')

# Override mostly_static to default True
env.CBAddVariables(
    BoolVariable('mostly_static', 'Link most libraries statically', 1))

conf = env.CBConfigure()

# Config vars
version = '8.0.0'
env.Replace(RESOURCES_NS = 'FAH::Client')
env.Replace(BUILD_INFO_NS = 'FAH::Client::BuildInfo')
env.Replace(PACKAGE_VERSION = version)
env.Replace(PACKAGE_AUTHOR = 'Joseph Coffland <joseph@cauldrondevelopment.com>')
env.Replace(PACKAGE_COPYRIGHT = '2019 foldingathome.org')
env.Replace(PACKAGE_HOMEPAGE = 'https://foldingathome.org/')
env.Replace(PACKAGE_LICENSE = 'https://www.gnu.org/licenses/gpl-2.0.txt')
env.Replace(PACKAGE_ORG = 'foldingathome.org')


if not env.GetOption('clean'):
    conf.CBConfig('compiler')
    conf.CBConfig('cbang')
    env.CBDefine('USING_CBANG') # Using CBANG macro namespace

    # OSX
    if env['PLATFORM'] == 'darwin':
        if not conf.CheckOSXFramework('SystemConfiguration'):
            raise Exception, 'Need SystemConfiguration framework'

    # Windows Systray
    if env['PLATFORM'] == 'win32': conf.CBRequireLib('shell32')

    if env['PLATFORM'] == 'posix':
        env.Append(PREFER_DYNAMIC = 'bz2 z m'.split())

conf.Finish()

# Client
Export('env')
client, libFAHClient = \
    SConscript('src/client.scons', variant_dir = 'build', duplicate = 0)
Default(client)

# Clean
Clean(client, ['build', 'config.log'])

# Dist
docs = ['README.md', 'CHANGELOG.md', 'LICENSE']
distfiles = docs + [client, 'images/fahlogo.png']
tar = env.TarBZ2Dist('fah-client', distfiles)
Alias('dist', tar)
AlwaysBuild(tar)

description = \
'''Folding@home is a distributed computing project using volunteered
computer resources.
'''

short_description = '''
This package contains the console client which is used to download
and run Folding@home work units.  To monitor and control the console
client it is best to use either Web Control via your browser, or the
Graphical User Interface called FAHControl, provided in a separate package.'''

description += short_description

if 'package' in COMMAND_LINE_TARGETS:
    if env['PLATFORM'] == 'win32' or int(env.get('cross_mingw', 0)):
        # Code sign key password
        path = os.environ.get('CODE_SIGN_KEY_PASS_FILE')
        if path is not None:
            code_sign_key_pass = open(path, 'r').read().strip()
        else: code_sign_key_pass = None

        if 'SIGNTOOL' in os.environ: env['SIGNTOOL'] = os.environ['SIGNTOOL']

    # Package
    pkg = env.Packager(
        'fahclient',

        version = version,
        maintainer = env['PACKAGE_AUTHOR'],
        vendor = env['PACKAGE_ORG'],
        url = env['PACKAGE_HOMEPAGE'],
        license = 'copyright',
        bug_url = 'https://github.com/FoldingAtHome/fah-client',
        summary = 'Folding@home Client',
        description = description,
        short_description = short_description,
        prefix = '/usr',
        icons = ('images/fahlogo.icns', 'images/fahlogo.png'),

        documents = docs,
        programs = [str(client[0])],
        desktop_menu = ['lin/fahclient.desktop'],
        changelog = 'CHANGELOG.md',

        nsi = 'install/win/fahclient.nsi',
        nsi_dll_deps = [str(client[0])],
        timestamp_url = 'http://timestamp.comodoca.com/authenticode',
        code_sign_key = os.environ.get('CODE_SIGN_KEY', None),
        code_sign_key_pass = code_sign_key_pass,

        deb_directory = 'install/debian',
        deb_section = 'science',
        deb_depends = 'debconf | debconf-2.0, libc6, bzip2, zlib1g',
        deb_pre_depends = 'adduser',
        deb_priority = 'optional',

        rpm_license = 'Restricted',
        rpm_group = 'Applications/Internet',
        rpm_pre_requires = 'shadow-utils',
        rpm_requires = 'expat, bzip2-libs',
        rpm_build = 'install/rpm/build',
        rpm_post = 'install/rpm/post',
        rpm_preun = 'install/rpm/preun',

        pkg_id = 'org.foldingathome.fahclient.pkg',
        pkg_scripts = 'install/osx/scripts',
        pkg_resources = 'install/osx/Resources',
        pkg_files = [[str(client[0]), 'usr/local/bin/', 0755],
                     ['install/osx/fahclient.url',
                      'Applications/Folding@home/fahclient.url', 0644],
                     ['install/osx/launchd.plist', 'Library/LaunchDaemons/' +
                      'org.foldingathome.fahclient.plist', 0644]],
        pkg_distribution = 'install/osx/distribution.xml',
        )

    AlwaysBuild(pkg)
    env.Alias('package', pkg)

    with open('package-description.txt', 'w') as f:
        f.write(short_description.strip())
