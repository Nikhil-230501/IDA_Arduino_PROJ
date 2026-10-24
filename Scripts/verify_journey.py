"""Run inside Unreal Editor with -ExecutePythonScript to exercise the real PIE media flow.

Only PIE instances are modified; the saved five-minute default is never changed.
Results: Saved/JourneyVerification.json. The editor stays open after verification.
"""
import json
import os
import time
import traceback
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)


class JourneyVerification:
    def __init__(self):
        self.results = []
        self.actor = None
        self.player = unreal.load_asset('/Game/Movies/MP_Experience')
        self.started = time.monotonic()
        with open(os.path.join(unreal.Paths.project_saved_dir(), 'JourneyVerification.json'), 'w', encoding='utf-8') as output:
            json.dump({'passed': None, 'status': 'running'}, output)
        self.steps = self.run()
        self.wait = None
        self.callback = unreal.register_slate_post_tick_callback(self.tick)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()

    def check(self, condition, message):
        if not condition:
            raise AssertionError(message)
        self.results.append(message)
        unreal.log('JOURNEY PASS: ' + message)

    def command(self, value):
        self.actor.call_method('HandleArduinoCommand', args=(value,))

    def active(self):
        return self.actor.get_editor_property('IsInteractionActive')

    def seconds(self):
        return unreal.MathLibrary.get_total_seconds(self.player.get_time())

    def held(self):
        # A freshly opened WMF source can be Ready/Stopped rather than Paused
        # before its first Play. Both must hold the frame until resume.
        return self.player.is_ready() and not self.player.is_playing()

    def finish(self, error=None):
        unreal.unregister_slate_post_tick_callback(self.callback)
        state = {'active': self.active() if self.actor else None,
                 'wants_paused': self.actor.get_editor_property('WantsPaused') if self.actor else None,
                 'playing': self.player.is_playing(), 'paused': self.player.is_paused(),
                 'url': self.player.get_url()}
        report = {'passed': error is None, 'checks': self.results, 'error': error, 'last_state': state}
        filename = os.path.join(unreal.Paths.project_saved_dir(), 'JourneyVerification.json')
        with open(filename, 'w', encoding='utf-8') as output:
            json.dump(report, output, indent=2)
        unreal.log('JOURNEY VERIFICATION: ' + json.dumps(report))
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

    def tick(self, delta):
        try:
            if time.monotonic() - self.started > 100:
                raise TimeoutError('Journey verification exceeded 100 seconds')
            if self.wait:
                predicate, deadline, message = self.wait
                if not predicate():
                    if time.monotonic() > deadline:
                        raise TimeoutError(message)
                    return
            predicate, timeout, message = next(self.steps)
            self.wait = (predicate, time.monotonic() + timeout, message)
        except StopIteration:
            self.finish()
        except Exception:
            self.finish(traceback.format_exc())

    def delay(self, seconds):
        until = time.monotonic() + seconds
        return lambda: time.monotonic() >= until, seconds + 3, 'Delay did not complete'

    def run(self):
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        yield lambda: subsystem.get_game_world() is not None, 30, 'PIE world did not start'
        world = subsystem.get_game_world()
        controller_class = unreal.load_class(None, '/Game/Arduino/BP_ArduinoController.BP_ArduinoController_C')
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, controller_class)
        self.check(len(actors) == 1, 'Exactly one journey controller exists')
        self.actor = actors[0]
        self.check('/Videos/Video1.mp4' in self.actor.get_editor_property('IdleSource').get_url().replace('\\', '/'),
                   'Idle source comes from the external Videos folder')
        self.check('/Videos/Video2.mp4' in self.actor.get_editor_property('InteractionSource').get_url().replace('\\', '/'),
                   'Main source comes from the external Videos folder')
        # Prevent test commands from competing with a real connected sensor.
        self.actor.get_editor_property('ArduinoSerial').disconnect()
        self.check(self.actor.get_editor_property('InactivityTimeoutMinutes') == 5.0, 'Saved timeout remains five minutes')
        yield lambda: self.player.is_playing() and not self.active(), 15, 'Idle did not start'
        self.check(self.player.is_looping(), 'Idle video loops')
        self.command('0')
        yield lambda: self.active() and self.player.is_playing(), 15, 'First single tap did not start main'
        self.check(not self.player.is_looping(), 'First single tap starts non-looping main')
        yield self.delay(0.6)
        self.command('0')
        yield lambda: self.player.is_paused(), 5, 'Second single tap did not pause'
        paused_at = self.seconds()
        yield self.delay(0.5)
        self.check(abs(self.seconds() - paused_at) < 0.15, 'Second single tap holds main video position')
        self.command('0')
        yield lambda: self.player.is_playing(), 5, 'Third single tap did not resume'
        self.check(self.active(), 'Third single tap resumes the same journey')
        self.command('1')
        yield lambda: not self.active() and self.player.is_playing() and self.player.is_looping(), 10, 'Double tap did not reset to idle'
        self.check(not self.actor.get_editor_property('WantsPaused'), 'Double tap while playing resets pause state and returns to idle')
        self.command('0')
        self.command('0')
        yield lambda: self.active() and self.held(), 10, 'Pause while opening was lost'
        held_at = self.seconds()
        yield self.delay(0.4)
        self.check(abs(self.seconds() - held_at) < 0.15, 'Single tap pause during opening is remembered')
        self.command('1')
        yield lambda: not self.active() and self.player.is_playing(), 10, 'Double tap from pause failed'
        self.check(self.player.is_looping(), 'Double tap while paused resets to looping idle')
        self.command('0')
        self.command('1')
        yield lambda: not self.active() and self.player.is_playing() and self.player.is_looping(), 10, 'Reset during opening failed'
        yield self.delay(0.4)
        self.check(self.player.get_url().endswith('Video1.mp4'), 'Double tap during opening stays on idle')
        self.command('1')
        yield lambda: not self.active() and self.player.is_playing(), 10, 'Double tap from idle failed'
        self.check(not self.active(), 'Repeated double tap waits on idle for another single tap')
        self.actor.set_editor_property('InactivityTimeoutMinutes', 0.05)
        self.command('0')
        yield lambda: self.active() and self.player.is_playing(), 10, 'Start after reset failed'
        yield self.delay(1.5)
        self.command('0')
        yield lambda: self.player.is_paused(), 5, 'Pause before timeout failed'
        yield self.delay(1.7)
        self.check(self.active(), 'Single tap refreshes inactivity countdown')
        yield lambda: not self.active() and self.player.is_playing(), 5, 'Paused inactivity did not return to idle'
        self.check(self.player.is_looping(), 'Editable inactivity timeout still returns to idle')
        self.command('0')
        yield lambda: self.active() and self.player.is_playing(), 10, 'Start failed'
        self.command('invalid')
        self.command('2')
        yield self.delay(0.3)
        self.check(self.active() and self.player.is_playing(), 'Unknown and retired long-hold command 2 are ignored')
        self.actor.call_method('MediaFinished')
        yield lambda: not self.active() and self.player.is_playing(), 10, 'End callback failed'
        self.check(self.player.is_looping(), 'Video completion still returns to idle')


journey_verification = JourneyVerification()
