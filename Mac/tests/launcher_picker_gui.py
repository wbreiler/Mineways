#!/usr/bin/env python3
"""Drive the unmodified packaged Mac app on an authorized, disposable CI desktop.

Requires the two complete installations and reference exports produced by
tools/test_launcher_real_packs.py. No Minecraft-world or mod-pack script command
is used: the native picker alone loads each scene in the same app process.
Never run this on the user's desktop. No permission or privacy settings change.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from import_mod_assets import decode_png
from test_launcher_real_packs import fingerprint, validate_gui_exports

APPLE = Path(__file__).with_suffix('.applescript')


def export_script(destination):
    return '\n'.join([
        'Set render type: Wavefront OBJ absolute indices', 'Center model: NO',
        'Export lesser blocks: YES', 'Individual blocks: YES',
        'File type: Export individual textures to directory individual-textures',
        'Selection location min to max: 0, 64, 0 to 15, 67, 15',
        'Focus view: 4, 2', 'Zoom: 16',
        'Export for Rendering: ' + str(destination / 'individual.obj'),
        'Export Schematic: ' + str(destination / 'roundtrip.schem'), ''])


def run(command, **kwargs):
    return subprocess.run(command, check=True, capture_output=True, text=True,
                          timeout=30, **kwargs).stdout.strip()


def ui(operation, pid=0, *args):
    return run(['/usr/bin/osascript', str(APPLE), operation, str(pid), *map(str, args)])


def wait_for(action, timeout=30):
    deadline = time.monotonic() + timeout
    last_error = None
    while time.monotonic() < deadline:
        try:
            result = action()
            if result:
                return result
        except subprocess.CalledProcessError as error:
            last_error = error.stderr
        time.sleep(0.3)
    raise AssertionError('Timed out waiting for the native app UI: ' + str(last_error))


def capture_window(pid, destination):
    # Enumerate window IDs, then capture only this app's normal window.
    javascript = '''ObjC.import('CoreGraphics');
function run(argv) {
 const windows = ObjC.deepUnwrap($.CGWindowListCopyWindowInfo(1, 0));
 const w = windows.find(w => w.kCGWindowOwnerPID === Number(argv[0]) && w.kCGWindowLayer === 0);
 if (!w) throw Error('The test app has no on-screen window');
 return String(w.kCGWindowNumber);
}'''
    window_id = run(['/usr/bin/osascript', '-l', 'JavaScript', '-e', javascript, str(pid)])
    run(['/usr/sbin/screencapture', '-x', '-o', '-l', window_id, str(destination)])
    width, height, pixels = decode_png(destination.read_bytes())
    if width < 400 or height < 250:
        raise AssertionError('The captured app window is unexpectedly small')
    # Exclude menu, status, and edges. An empty/checkerboard map has few colors.
    colors = {tuple(pixels[4 * (y * width + x):4 * (y * width + x) + 4])
              for y in range(height // 4, 3 * height // 4)
              for x in range(width // 4, 3 * width // 4)}
    if len(colors) < 16:
        raise AssertionError('The central map capture is empty or has too few colors')
    return {'window_id': window_id, 'width': width, 'height': height, 'central_colors': len(colors)}


def smoke(package, installations, output):
    if (sys.platform != 'darwin' or os.environ.get('GITHUB_ACTIONS') != 'true'
            or os.environ.get('RUNNER_OS') != 'macOS'):
        raise RuntimeError('This UI test runs only on a disposable GitHub macOS runner')
    # Refuse absent authorization before launching Mineways. Never grant it here.
    ui('preflight')
    app = package / 'Mineways.app'
    before = fingerprint(app)
    run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)])
    cases = sorted(installations.glob('*/installation.json'))
    if len(cases) != 2 or not (installations / 'validation.json').is_file():
        raise ValueError('Both verified real-launcher installations are required')
    saves_before = {}
    for setup_path in cases:
        setup = json.loads(setup_path.read_text())
        saves_before[str(Path(setup['world']).parent)] = fingerprint(Path(setup['world']).parent)
    output.mkdir(parents=True, exist_ok=False)
    results = []
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('DYLD_', 'PYTHON'))}
    with (output / 'app.log').open('w') as log:
        process = subprocess.Popen([str(app / 'Contents/MacOS/mineways')], cwd=package,
                                   env=environment, stdout=log, stderr=log)
        try:
            wait_for(lambda: ui('title', process.pid))
            for setup_path in cases:
                setup = json.loads(setup_path.read_text())
                destination = output / setup_path.parent.name
                destination.mkdir()
                script = destination / 'export-current-scene.mwscript'
                script.write_text(export_script(destination), encoding='utf-8')
                ui('menu', process.pid, 'Choose Modpack Folder…')
                wait_for(lambda: ui('choose', process.pid, setup['instance'], 'Choose',
                                    'Choose a Prism or CurseForge modpack instance'))
                wait_for(lambda: ui('choose', process.pid, setup['world'], 'Choose',
                                    "Choose a world from this modpack's saves folder"))
                wait_for(lambda: 'Original exporter test' in ui('title', process.pid), timeout=600)
                ui('menu', process.pid, 'Import Settings...')
                wait_for(lambda: ui('choose', process.pid, script, 'Open',
                                    'Import Settings from model or script file'))
                alerts = 0
                deadline = time.monotonic() + 120
                while time.monotonic() < deadline:
                    status = ui('poll-export', process.pid)
                    if status == 'dismissed-export':
                        alerts += 1
                    if status == 'main' and alerts == 2:
                        break
                    time.sleep(0.3)
                else:
                    raise AssertionError('The two export success dialogs did not complete')
                time.sleep(1)  # Allow queued map drawing after the modal script finishes.
                results.append({'fixture': setup_path.parent.name, 'pid': process.pid,
                                'title': ui('title', process.pid), 'export_success_dialogs': alerts,
                                'map': capture_window(process.pid, destination / 'map.png')})
            validate_gui_exports(installations, output)
        finally:
            process.terminate()  # Only the exact process created above.
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)
    if fingerprint(app) != before:
        raise AssertionError('The signed app bundle changed during the UI test')
    run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)])
    for saves, original in saves_before.items():
        if fingerprint(Path(saves)) != original:
            raise AssertionError('The native picker modified an instance save')
    evidence = {'native_picker': results, 'same_process': True, 'signed_app_unchanged': True,
                'saves_unchanged': True, 'permissions_changed': False}
    (output / 'gui-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence), flush=True)


class Checks(unittest.TestCase):
    def test_script_cannot_load_or_clear_a_scene(self):
        script = export_script(Path('/tmp/output with spaces'))
        self.assertNotIn('Minecraft world:', script)
        self.assertNotIn('Mod pack:', script)
        self.assertNotIn('Close', script)
        self.assertIn('Export for Rendering: /tmp/output with spaces/individual.obj', script)
        self.assertIn('Export Schematic: /tmp/output with spaces/roundtrip.schem', script)

    def test_refuse_local_desktop(self):
        previous = os.environ.pop('GITHUB_ACTIONS', None)
        try:
            with self.assertRaisesRegex(RuntimeError, 'disposable GitHub'):
                smoke(Path('/absent'), Path('/absent'), Path('/absent'))
        finally:
            if previous is not None:
                os.environ['GITHUB_ACTIONS'] = previous


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path, nargs='?')
    parser.add_argument('--installations', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        unittest.main(argv=[__file__])
    elif args.package and args.installations and args.output:
        smoke(args.package.resolve(), args.installations.resolve(), args.output.resolve())
    else:
        parser.error('PACKAGE, --installations and --output are required')


if __name__ == '__main__':
    main()
