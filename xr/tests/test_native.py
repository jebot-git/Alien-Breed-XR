#!/usr/bin/env python3
"""Isolated real GPU / OpenXR API-fixture test, without a headset connection."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
BUILD=Path(os.environ.get('ABXR_BUILD_DIR',ROOT/'build-xr'))
OUT=ROOT/'artifacts/native-test'
OUT.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ab3d-native-') as directory:
    env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','XDG_RUNTIME_DIR':directory}
    for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','DISPLAY','LD_PRELOAD'):env.pop(key,None)
    binary=BUILD/'alien-breed-xr'
    for level in range(1,17):
        report=OUT/f'level-{level}.json'
        report.unlink(missing_ok=True)
        result=subprocess.run([binary,'--desktop','--level',str(level),'--frames','12','--report',report],env=env,capture_output=True,text=True,timeout=35)
        (OUT/f'level-{level}.log').write_text(result.stdout+result.stderr)
        assert result.returncode==0,(level,result.returncode,result.stderr[-2000:])
        data=json.loads(report.read_text())
        print('level',level,'returncode',result.returncode,'triangles',data['gpu_triangles'],flush=True)
        assert result.returncode==0 and data['frames']==12 and data['gpu_triangles']>100 and data['level']==level,data
        assert not data['domed_managed']
    eyes=OUT/'eyes.txt';eyes.unlink(missing_ok=True)
    env.update(LD_PRELOAD=str(BUILD/'libab3d-mock-openxr.so'),ABXR_TEST_EYES=str(eyes))
    result=subprocess.run([binary,'--xr','--frames','110','--report',OUT/'xr.json','--capture',OUT/'mirror.ppm'],env=env,capture_output=True,text=True,timeout=35)
    log=result.stdout+result.stderr;(OUT/'xr.log').write_text(log)
    assert result.returncode==0 and 'OpenXR frames=110 stereo_frames=110' in log,log[-3000:]
    data=json.loads((OUT/'xr.json').read_text())
    assert data['xr_entries']==1 and data['controller_shots']>10 and data['xr_suspends']==0,data
    hashes=[line.split() for line in eyes.read_text().splitlines()]
    assert len(hashes)==110 and sum(left!=right for _,left,right in hashes)>100
    assert len({left for _,left,_ in hashes})>50
    (OUT/'report.json').write_text(json.dumps({'passed':True,'levels':list(range(1,17)),'stereo_frames':len(hashes),'controller_shot_overrides':data['controller_shots'],'distinct_left_views':len({left for _,left,_ in hashes}),'physical_headset_tested':False,'renderer':data['renderer']},indent=2))
    print((OUT/'report.json').read_text())
