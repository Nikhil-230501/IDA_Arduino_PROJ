"""Run in the editor to arrange the journey using measured Slate node bounds.

Applies single-tap toggle and double-tap idle reset, compiles and saves the Blueprint, then exercises it in PIE.
"""
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
if not unreal.JourneySetupCommandlet.tidy_journey_blueprint(True):
    raise RuntimeError('Journey reset/layout update failed; inspect the Unreal log')
exec(compile((Path(__file__).parent / 'verify_journey.py').read_text(encoding='utf-8'),
             str(Path(__file__).parent / 'verify_journey.py'), 'exec'))
