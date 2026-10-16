#!/usr/bin/env python3
"""Qualify the selected dirty source without changing its UI, imports or retained builds.

Use fresh external --work-root and --report-root. Successful scratch is caller-owned
and disposable after consumer checks; failures retain their exact inputs and logs.
This proves the current imported baseline, not parity with newer workspace modules.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

FALLBACKS = ('core/core_scene_view', 'core/core_viewport3d',
             'core/core_screen_pick', 'kit/kit_viewport3d')
ARTIFACT_ROOTS = {'.cache', 'Users'}


def digest(path):
    if path.is_symlink():
        return 'symlink:' + os.readlink(path)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def visible(source):
    names = subprocess.check_output(['git', '-C', str(source), 'ls-files', '-co',
                                     '--exclude-standard', '-z'], text=True)
    return sorted(set(filter(None, names.split('\0'))))


def snapshot(source, destination):
    files, excluded = {}, {}
    for name in visible(source):
        path = source / name
        if Path(name).parts[0] in ARTIFACT_ROOTS:
            if path.is_symlink() or path.is_file():
                excluded[name] = digest(path)
            continue
        if path.is_symlink():
            raise RuntimeError('Explicit symlink handling required: ' + name)
        if not path.is_file():
            continue
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        files[name] = digest(target)
    return files, excluded


def fresh(path, source):
    path = path.resolve()
    if path == source or source in path.parents:
        raise RuntimeError('Proof roots must be outside the source checkout')
    path.mkdir(parents=True, exist_ok=False)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lifecycle', action='store_true', help='Qualify shared native lifecycle after foundation integration')
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--shared-root', required=True, type=Path)
    parser.add_argument('--work-root', required=True, type=Path)
    parser.add_argument('--report-root', required=True, type=Path)
    args = parser.parse_args()
    source, shared = args.source_root.resolve(), args.shared_root.resolve()
    work, report = args.work_root.resolve(), args.report_root.resolve()
    if work == report or work in report.parents or report in work.parents:
        raise RuntimeError('Work/report roots must be separate')
    work = fresh(work, source)
    report = fresh(report, source)
    copied = work / 'source'
    files, excluded = snapshot(source, copied)
    fallback_hashes = {}
    for name in FALLBACKS:
        origin, target = shared / name, work / 'shared' / name
        fallback_hashes[name] = {}
        for path in origin.rglob('*'):
            if not path.is_file() or 'build' in path.relative_to(origin).parts:
                continue
            if path.is_symlink():
                raise RuntimeError('Explicit fallback symlink handling required: ' + str(path))
            relative = path.relative_to(origin)
            out = target / relative
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, out)
            fallback_hashes[name][str(relative)] = digest(out)
    helper = shared.parent / 'bin/desktop_release_target_contract.sh'
    frozen_helper = work / 'bin/desktop_release_target_contract.sh'
    frozen_helper.parent.mkdir(parents=True)
    shutil.copy2(helper, frozen_helper)
    helper_hash = digest(helper)
    write(report / 'source-manifest.json', dict(source=str(source), files=files,
          excluded_artifacts=excluded, fallback_root=str(shared), fallbacks=fallback_hashes,
          target_helper=dict(path=str(helper), sha256=helper_hash)))
    env = {k: v for k, v in os.environ.items() if not k.startswith(('LINE_DRAWING', 'SDL_', 'VK_', 'DYLD_'))}
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    make = ['make', '-s', 'BUILD_TOOLCHAIN=clang',
            'CODEWORK_WORKSPACE_ROOT=' + str(shared.parent),
            'TARGET_CONTRACT_HELPER=' + str(frozen_helper),
            'SHARED_WORKSPACE_DIR=' + str(work / 'shared')]
    results = []

    def run(name, command, cwd=copied, selected_env=env):
        with (report / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=cwd, env=selected_env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=600)
        results.append(dict(name=name, exit=result.returncode))
        write(report / 'result.json', results)
        if result.returncode:
            raise RuntimeError(name + ' failed; see ' + str(report / (name + '.log')))

    try:
        run('build', make + ['all'])
        run('targeted', make + ['test-input-policy', 'test-shared-theme-font-adapter'])
        run('package-contract', ['bash', 'tests/run_main_edit_package_contract_checks.sh'])
        run('unit', make + ['test'])
        run('agent-scene', make + ['agent-scene-smoke'])
        spec = importlib.util.spec_from_file_location('rollout', copied / 'tools/verify-vulkan-rollout.py')
        rollout = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(rollout)
        binary = copied / 'build/toolchains/clang/bin/LineDrawing'
        for name, mode in [('menu', ''), ('editor', 'editor'), ('native', None)]:
            cwd = work / name
            cwd.mkdir()
            shutil.copytree(copied / 'config', cwd / 'config')
            shutil.copytree(copied / 'include/fonts', cwd / 'include/fonts')
            shutil.copytree(copied / 'third_party/codework_shared/assets', cwd / 'shared/assets')
            selected = dict(env)
            if mode is None:
                selected.update(rollout.validation_environment())
                # The validation helper inherits the process environment; replace
                # app controls with the sanitized baseline environment explicitly.
                for key in list(selected):
                    if key.startswith(('LINE_DRAWING', 'SDL_')):
                        selected.pop(key)
                initial, resized = work / 'initial.bmp', work / 'resized.bmp'
                selected.update(LINE_DRAWING_VULKAN_ROLLOUT_INITIAL_CAPTURE=str(initial),
                                LINE_DRAWING_VULKAN_ROLLOUT_RESIZED_CAPTURE=str(resized),
                                LINE_DRAWING_VULKAN_ROLLOUT_MIN_SCALE='1.5')
                run(name, [str(binary), '--vulkan-rollout-self-test'], cwd, selected)
                text = (report / (name + '.log')).read_text()
                assert 'status=pass' in text and '[vk_runtime validation]' not in text
                for stage in ('startup', 'resized', 'restart'):
                    assert 'stage=' + stage + ' runtime=' in text
                assert rollout.bmp_evidence(initial)[:2] != rollout.bmp_evidence(resized)[:2]
            else:
                capture = work / (name + '.bmp')
                selected.update(LINE_DRAWING_VISUAL_ARTIFACT=str(capture),
                                LINE_DRAWING_VISUAL_ARTIFACT_MODE=mode)
                run(name, [str(binary)], cwd, selected)
                rollout.bmp_evidence(capture)
        if args.lifecycle:
            stages = ['initial', 'resize', 'fullscreen', 'windowed', 'hidden', 'shown', 'minimized', 'restored']
            for mode in ('menu', 'editor'):
                name = 'window-' + mode
                cwd = work / name
                cwd.mkdir()
                for origin, target in [('config', 'config'), ('include/fonts', 'include/fonts'),
                                       ('third_party/codework_shared/assets', 'shared/assets')]:
                    shutil.copytree(copied / origin, cwd / target)
                selected = dict(env)
                selected.update(rollout.validation_environment())
                for key in list(selected):
                    if key.startswith(('LINE_DRAWING', 'SDL_')):
                        selected.pop(key)
                selected['CODEWORK_WINDOW_LIFECYCLE_PROOF'] = str(cwd)
                if mode == 'editor':
                    selected['LINE_DRAWING_WINDOW_PROOF_EDITOR'] = '1'
                run(name, [str(binary)], cwd, selected)
                text = (report / (name + '.log')).read_text()
                observed = [line.split('stage=', 1)[1].split()[0] for line in text.splitlines()
                            if line.startswith('WINDOW_LIFECYCLE ') and 'stage=' in line and 'status=pass' in line]
                assert observed == stages, (mode, observed)
                assert 'status=complete' in text and 'WINDOW_DOCUMENT status=pass' in text
                assert '[vk_runtime validation]' not in text
                images = sorted(cwd.glob('line-drawing-*.bmp'))
                assert len(images) == 6
                for image in images:
                    rollout.bmp_evidence(image)
        print('LINE UI BASELINE PASS: fresh build, focused/unit/export contracts, menu/editor Vulkan frames and validated resize/restart')
    finally:
        changed = [n for n, expected in {**files, **excluded}.items()
                   if not ((source / n).is_file() or (source / n).is_symlink()) or digest(source / n) != expected]
        for name, hashes in fallback_hashes.items():
            changed += [str(shared / name / n) for n, expected in hashes.items()
                        if digest(shared / name / n) != expected]
        if digest(helper) != helper_hash:
            changed.append(str(helper))
        write(report / 'preservation.json', dict(original_source_unchanged=not changed, changed=changed))
        if changed:
            raise RuntimeError('Source/input drift during qualification: ' + ', '.join(changed[:8]))


if __name__ == '__main__':
    main()
