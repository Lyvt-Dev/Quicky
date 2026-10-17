#!/usr/bin/env python3
"""Build a Debian package with explicit shared-library dependencies."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import platform

parser = argparse.ArgumentParser()
parser.add_argument('--build', default='build')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
binary = root / args.build / 'Quicky'
if not binary.is_file():
    parser.error('Build Quicky first')
dist = root / 'dist'
dist.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='quicky-package-') as scratch:
    stage = Path(scratch)
    (stage / 'DEBIAN').mkdir()
    (stage / 'usr/bin').mkdir(parents=True)
    shutil.copy2(binary, stage / 'usr/bin/quicky')
    # Local SDK prefixes must never leak into the distributed executable.
    subprocess.run(['patchelf', '--remove-rpath', str(stage / 'usr/bin/quicky')], check=True)
    (stage / 'usr/bin/quicky').chmod(0o755)
    docs = stage / 'usr/share/doc/quicky'
    docs.mkdir(parents=True)
    shutil.copy2(root / 'THIRD_PARTY_NOTICES.md', docs)
    shutil.copytree(root / 'licenses', docs / 'licenses')
    desktop = stage / 'usr/share/applications'
    desktop.mkdir(parents=True)
    (desktop / 'quicky.desktop').write_text('[Desktop Entry]\nType=Application\nName=Quicky\nExec=quicky\nIcon=quicky\nTerminal=false\nCategories=Network;FileTransfer;\n')
    icons = stage / 'usr/share/icons/hicolor/scalable/apps'
    icons.mkdir(parents=True)
    shutil.copy2(root / 'packaging/quicky.svg', icons / 'quicky.svg')
    qt_version = (root / args.build / 'quicky-qt-version.txt').read_text().strip()
    libc_version = platform.libc_ver()[1]
    gcc_version = subprocess.check_output(['g++', '-dumpfullversion'], text=True).strip()
    (stage / 'DEBIAN/control').write_text(f'''Package: quicky
Version: 0.1.0~preview1
Section: net
Priority: optional
Architecture: amd64
Maintainer: Quicky contributors <noreply@github.com>
Depends: libc6 (>= {libc_version}), libstdc++6 (>= {gcc_version}), libqt6core6t64 (>= {qt_version}), libqt6gui6 (>= {qt_version}), libqt6network6 (>= {qt_version}), libqt6qml6 (>= {qt_version}), libqt6quick6 (>= {qt_version}), libqt6quickcontrols2-6 (>= {qt_version}), libqt6concurrent6 (>= {qt_version}), libssl3t64, qt6-qpa-plugins, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-dialogs, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qml6-module-qtqml-models, qml6-module-qtquick-templates, qml6-module-qtquick-nativestyle
Description: Native encrypted local file sharing (preview)
 Direct TLS 1.3 transfers with QR invitations and explicit receiver consent.
''')
    output = dist / 'Quicky-0.1.0-preview.1-linux-amd64.deb'
    stage.chmod(0o755)
    for entry in stage.rglob('*'):
        entry.chmod(0o755 if entry.is_dir() or entry == stage / 'usr/bin/quicky' else 0o644)
    subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(stage), str(output)], check=True)
import hashlib
output.with_suffix(output.suffix + '.sha256').write_text(hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
print(output)
