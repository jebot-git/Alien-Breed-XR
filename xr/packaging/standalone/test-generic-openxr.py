#!/usr/bin/env python3
"""Check generic EGL and GLX OpenXR without touching a user desktop or headset."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--game',required=True,type=Path)
p.add_argument('--runtime',required=True,type=Path)
p.add_argument('--assets',type=Path)
p.add_argument('--output',type=Path)
a=p.parse_args();meta=json.loads((Path(__file__).parent/'release.json').read_text());pkg=meta['package']
results=[]
with tempfile.TemporaryDirectory(prefix='standalone-openxr-') as temp:
 base=Path(temp);rd,wr=os.pipe()
 with (base/'xvfb.log').open('w') as log:
  server=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1280x800x24','-nolisten','tcp','-noreset'],pass_fds=(wr,),stdout=log,stderr=log)
  os.close(wr)
  with os.fdopen(rd) as pipe:number=pipe.readline().strip()
  assert number,'Xvfb failed'
  try:
   for binding in ['egl','glx']:
    report=base/(binding+'.json');frames=base/(binding+'-frames')
    env=dict(os.environ,DISPLAY=':'+number,SDL_AUDIODRIVER='dummy',XDG_RUNTIME_DIR=temp,XDG_CONFIG_HOME=temp+'/config',XDG_STATE_HOME=temp+'/state',XDG_DATA_HOME=temp+'/data',LIBGL_ALWAYS_SOFTWARE='1',LD_PRELOAD=str(a.runtime.resolve()),DE_XR_TEST_GENERIC='1',TEMPEST_TEST_FRAMES=str(frames),ABXR_TEST_EYES=str(frames),ABXR_TEST_EYE_SIZE='256')
    for k in ['WAYLAND_DISPLAY','WAYLAND_SOCKET','SDL_VIDEODRIVER','SDL_VIDEO_X11_FORCE_EGL','DE_XR_TEST_GLX','ABXR_CONFIG','ABXR_SAVE_DIR']:env.pop(k,None)
    if binding=='egl':env['SDL_VIDEODRIVER']='offscreen'
    else:env['DE_XR_TEST_GLX']='1'
    command=[str(a.game.resolve()),'--xr','--frames','60','--report',str(report)]
    if pkg=='tempest-xr':command+=['--demo','--no-audio']
    elif pkg=='alien-breed3d2-xr':
     command+=['--standalone','--test-play','--nosound']
     if a.assets:command+=['--assets',str(a.assets.resolve())]
    result=subprocess.run(command,cwd=temp,env=env,capture_output=True,text=True,timeout=40)
    assert result.returncode==0,result.stdout+result.stderr
    data=json.loads(report.read_text())
    if a.output:a.output.with_name(pkg+'-'+binding+'.log').write_text(result.stdout+result.stderr)
    if pkg!='tempest-xr':assert data['frames']==60,data
    if pkg=='tempest-xr':
     assert data['xr_entries']==1 and not data['managed'] and data['gl_error']==0,data
     assert sum(line.endswith(' 1') for line in frames.read_text().splitlines())>=58
    elif pkg=='alien-breed-xr':assert data['xr_entries']==1 and not data['domed_managed'],result.stderr
    else:assert data['xr'] and data['eye_frames']==120 and data['missing_assets']==0,data
    if binding=='glx':
     assert 'GLX' not in result.stderr or 'error' not in result.stderr.lower(),result.stderr
     if 'video_driver' in data:assert data['video_driver']=='x11',data
    results.append(dict(binding=binding,frames=data.get('frames',60),vendor_extensions=False,domed=False,result='passed'))
  finally:
   server.terminate();server.wait(timeout=5)
print(json.dumps(results,indent=2))
if a.output:a.output.write_text(json.dumps(results,indent=2)+'\n')
