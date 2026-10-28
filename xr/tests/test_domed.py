#!/usr/bin/env python3
"""Private DomeD lease/focus/background test; never controls the active session."""
import json,os,signal,socket,subprocess,sys,tempfile,threading,time,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'artifacts/domed-test';OUT.mkdir(parents=True,exist_ok=True)
if not os.environ.get('ABXR_TEST_BUS'):
    os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,*sys.argv],{**os.environ,'ABXR_TEST_BUS':'1'})
signal.pthread_sigmask(signal.SIG_SETMASK,[])
children=[];logs=[];stop=threading.Event()
def start(command,env,name,**kwargs):
    log=(OUT/f'{name}.log').open('w');logs.append(log)
    child=subprocess.Popen([str(v) for v in command],env=env,stdout=log,stderr=log,**kwargs);children.append(child);return child

def wait(fn,timeout=25):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            value=fn()
            if value:return value
        except (OSError,ValueError,KeyError,StopIteration):pass
        time.sleep(.05)
    raise AssertionError('Timed out; inspect '+str(OUT))
try:
  with tempfile.TemporaryDirectory(prefix='abxr-dome-') as directory:
    runtime=Path(directory);runtime.chmod(0o700)
    rd,wr=os.pipe();start(['Xvfb','-displayfd',str(wr),'-screen','0','1400x1000x24','-nolisten','tcp'],os.environ,'xvfb',pass_fds=(wr,));os.close(wr)
    with os.fdopen(rd) as stream:display=':'+stream.readline().strip()
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    env={**os.environ,'DISPLAY':display,'XDG_RUNTIME_DIR':directory,'DOME_PREVIEW_PORT':str(port),'DOME_LOGIN_SESSION':'0','DOME_START_XR':'0','DOME_APPEARANCE_SERVICE':'0','DOME_RENDERER':'gles','LIBGL_ALWAYS_SOFTWARE':'1','SDL_AUDIODRIVER':'dummy','SDL_VIDEODRIVER':'wayland','XDG_CURRENT_DESKTOP':'Dome'}
    for name in ('CONFIG','DATA','STATE','CACHE'):env[f'XDG_{name}_HOME']=directory+'/'+name.lower()
    for name in ('TRAY','KDECONNECT','KWALLET','POLKIT','IME'):env['DOME_START_'+name]='0'
    for name in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','LD_PRELOAD'):env.pop(name,None)
    start([ROOT.parent/'DomeD/target/release/dome-compositor','--backend','nested'],env,'compositor')
    http=urllib.request.build_opener(urllib.request.ProxyHandler({}))
    def request(path,body=None):
        req=urllib.request.Request(f'http://127.0.0.1:{port}'+path,data=None if body is None else json.dumps(body).encode(),headers={'Content-Type':'application/json'})
        with http.open(req,timeout=3) as response:return json.load(response)
    def scene():return request('/api/state')['scene']
    def native():return request('/api/native-xr')
    wait(scene)
    def realm(name):return next((p.name.removesuffix('.dome-session.json') for p in runtime.glob('*.dome-session.json') if json.loads(p.read_text())['realm']==name),None)
    desktop=wait(lambda:realm('desktop'));xr=wait(lambda:realm('xr'))
    def heartbeat():
        while not stop.wait(.1):
            try:request('/api/xr-session',{'id':32471,'visible':True,'focused':True,'head':{'position_m':[0,1.6,0],'orientation_xyzw':[0,0,0,1]},'horizontal_fov_deg':90,'mouse_viewport':[-.7,.7,-.65,.65]})
            except OSError:pass
    threading.Thread(target=heartbeat,daemon=True).start()
    wait(lambda:request('/api/xr-control')['input_realm']=='xr')
    binary=ROOT/'build-xr/alien-breed-xr'
    background=start([binary,'--desktop','--frames','100000'],{**env,'WAYLAND_DISPLAY':desktop},'background')
    old=wait(lambda:next((w for w in scene()['windows'] if w.get('client_pid')==background.pid and w.get('title')=='Alien Breed 3D XR'),None));oldid=old['id']
    request('/api/action',{'op':'window_visibility','id':oldid,'desktop':True,'xr':True})
    request('/api/action',{'op':'focus','id':oldid})
    request('/api/action',{'op':'xr_presentation','top':False,'bottom':True,'desktop':True})
    before=scene();report=OUT/'game.json';report.unlink(missing_ok=True)
    game=start([binary,'--xr','--frames','100000','--report',report],{**env,'WAYLAND_DISPLAY':xr,'LD_PRELOAD':str(ROOT/'build-xr/libab3d-mock-openxr.so')},'game')
    gamewin=wait(lambda:next((w for w in scene()['windows'] if w.get('client_pid')==game.pid and w.get('title')=='Alien Breed 3D XR'),None));gid=gamewin['id']
    def active():
        snapshot={'native':native(),'scene':scene(),'expected':gid}
        (OUT/'state.json').write_text(json.dumps(snapshot,indent=2))
        return snapshot['native'].get('active') and snapshot['native'].get('owner')==gid
    wait(active)
    wait(lambda:report.exists() and json.loads(report.read_text())['xr_entries']==1)
    print('PASS native XR ownership and input/background handoff',flush=True)
    request('/api/action',{'op':'focus','id':oldid})
    wait(lambda:not native().get('active'))
    wait(lambda:json.loads(report.read_text())['paused'] and json.loads(report.read_text())['xr_suspends']>=1)
    assert scene()['presentation']==before['presentation']
    print('PASS focus loss suspends XR, pauses gameplay and restores background',flush=True)
    request('/api/action',{'op':'focus','id':gid})
    from Xlib import X, XK, display as xdisplay
    from Xlib.ext import xtest
    xd=xdisplay.Display(display)
    host=wait(lambda:next((w for w in xd.screen().root.query_tree().children if str(w.get_wm_name() or '').startswith('Dome')),None))
    host.set_input_focus(X.RevertToParent,X.CurrentTime);xd.sync();time.sleep(.2)
    code=xd.keysym_to_keycode(XK.string_to_keysym('Return'))
    xtest.fake_input(xd,X.KeyPress,code);xd.sync();time.sleep(.08)
    xtest.fake_input(xd,X.KeyRelease,code);xd.sync()
    wait(lambda:native().get('active') and json.loads(report.read_text())['xr_entries']==2)
    wait(lambda:not json.loads(report.read_text())['paused'])
    xd.close()
    print('PASS explicit resume reacquires native XR input and stereo session',flush=True)
    game.terminate();assert game.wait(timeout=10)==0
    wait(lambda:not native().get('active'))
    (OUT/'report.json').write_text(json.dumps({'passed':True,'native_lease':True,'background_restored':True,'focus_loss_pauses':True,'explicit_resume':True,'graceful_exit':True,'active_session_touched':False},indent=2))
finally:
    stop.set()
    for child in reversed(children):
        if child.poll() is None:child.terminate()
    for child in reversed(children):
        try:child.wait(timeout=8)
        except subprocess.TimeoutExpired:child.kill();child.wait()
    for log in logs:log.close()
