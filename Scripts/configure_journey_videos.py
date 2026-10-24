"""Install/tidy the startup JSON loader, verify path selection, then run actual playback checks."""
from pathlib import Path
import json
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
config = Path(unreal.Paths.project_dir()) / 'IDA_Videos.json'
original = config.read_bytes()
checks = []

def load():
    result = unreal.JourneyVideoLibrary.load_video_configuration()
    # Unreal's Python wrapper uses a Boolean success return to make outputs optional.
    return (True, *result) if result is not None else (False, None, None)

try:
    success, idle, main = load()
    assert success and idle.get_url().endswith('Videos/Video1.mp4') and main.get_url().endswith('Videos/Video2.mp4')
    checks.append('Default JSON resolves both external videos')
    config.write_text(json.dumps({'idleVideo': 'Videos/Video2.mp4', 'mainVideo': 'Videos/Video1.mp4'}), encoding='utf-8')
    success, idle, main = load()
    assert success and idle.get_url().endswith('Videos/Video2.mp4') and main.get_url().endswith('Videos/Video1.mp4')
    checks.append('Changing JSON selects different videos without rebuilding')
    config.write_text(json.dumps({'idleVideo': str((config.parent / 'Videos/Video1.mp4').resolve()),
                                 'mainVideo': str((config.parent / 'Videos/Video2.mp4').resolve())}), encoding='utf-8')
    assert load()[0]
    checks.append('Absolute paths are supported')
    config.write_text('{invalid', encoding='utf-8')
    assert not load()[0]
    checks.append('Malformed JSON is rejected')
    config.write_text(json.dumps({'idleVideo': 'Videos/missing.mp4', 'mainVideo': 'Videos/Video2.mp4'}), encoding='utf-8')
    assert not load()[0]
    checks.append('Missing video is rejected without partial source selection')
    config.write_text('{}', encoding='utf-8')
    assert not load()[0]
    checks.append('Missing required keys are rejected')
finally:
    config.write_bytes(original)

(Path(unreal.Paths.project_saved_dir()) / 'VideoConfigVerification.json').write_text(
    json.dumps({'passed': True, 'checks': checks}, indent=2), encoding='utf-8')
if not unreal.JourneySetupCommandlet.tidy_journey_blueprint(True):
    raise RuntimeError('Could not compile/save the configured Blueprint')
exec(compile((Path(__file__).parent / 'verify_journey.py').read_text(encoding='utf-8'),
             str(Path(__file__).parent / 'verify_journey.py'), 'exec'))
