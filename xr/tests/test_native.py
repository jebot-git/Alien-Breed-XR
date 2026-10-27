#!/usr/bin/env python3
"""Isolated real GPU / OpenXR API-fixture test, without a headset connection."""
import json
import os
from pathlib import Path
import re
import subprocess
import shutil
import tempfile
ROOT=Path(__file__).resolve().parents[2]
BUILD=Path(os.environ.get('ABXR_BUILD_DIR',ROOT/'build-xr'))
OUT=ROOT/'artifacts/native-test'
OUT.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ab3d-native-') as directory:
    env={**os.environ,'SDL_VIDEODRIVER':'offscreen','SDL_AUDIODRIVER':'dummy','XDG_RUNTIME_DIR':directory}
    env['XDG_STATE_HOME']=directory+'/state'
    for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','DISPLAY','LD_PRELOAD','ABXR_SAVE_DIR','ABXR_TEST_MENU','ABXR_TEST_WALL'):env.pop(key,None)
    # Use isolated, all-original-weapons fixtures without editing the user's INI.
    game=Path(directory)/'game';game.mkdir()
    binary=game/'alien-breed-xr';shutil.copy2(BUILD/'alien-breed-xr',binary)
    (game/'data').symlink_to(BUILD/'data',target_is_directory=True)
    (game/'fonts').symlink_to(BUILD/'fonts',target_is_directory=True)
    config=(ROOT/'ab3d.ini.template').read_text().replace('all_weapons=0','all_weapons=1').replace('flamethrower_weapon=0','flamethrower_weapon=1')
    (game/'ab3d.ini').write_text(config)
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
        assert data['vector_objects_rendered']>0 and data['vector_meshes_uploaded']>0,data
    eyes=OUT/'eyes.txt';eyes.unlink(missing_ok=True)
    env.update(LD_PRELOAD=str(BUILD/'libab3d-mock-openxr.so'),ABXR_TEST_EYES=str(eyes))
    result=subprocess.run([binary,'--xr','--frames','110','--report',OUT/'xr.json','--capture',OUT/'mirror.ppm'],env=env,capture_output=True,text=True,timeout=35)
    log=result.stdout+result.stderr;(OUT/'xr.log').write_text(log)
    assert result.returncode==0 and 'OpenXR frames=110 stereo_frames=110' in log,log[-3000:]
    data=json.loads((OUT/'xr.json').read_text())
    assert data['use_actions']==2,data
    assert data['weapon_previous_actions']==1 and data['weapon_next_actions']==1 and data['weapon_changes']>=2,data
    assert data['physical_height_min_m']<1.2 and data['engine_height_min_m']<1.2 and data['engine_height_m']>1.4,data
    assert data['movement_samples']>0 and data['movement_first_world_xz'][0]<-.3 and abs(data['movement_first_world_xz'][1])<.02,data
    assert data['smooth_turn'] and 1<data['virtual_yaw_deg']<15,data
    assert data['xr_entries']==1 and data['controller_shots']>4 and data['xr_suspends']==0,data
    assert data['weapon_models_loaded']==5 and data['weapon_draws']==220 and data['weapon_gpu_triangles']>100,data
    assert data['weapon_slots_drawn'] & 1 and data['weapon_slots_drawn'] != 1,data
    assert data['selectable_weapon_mask']==0x97 and not data['flamethrower_enabled'],data
    assert data['weapon_name']=='Mark IV Pulse Rifle',data
    assert data['wall_blocked_frames']==0 and data['wall_fade']==0,data
    assert data['actual_shots_fired']>0 and data['actual_shots_fired']<12,data
    assert data['shot_billboards_rendered']>0,data
    assert data['aim_laser_draws']>100,data
    assert abs(data['simulation_vblanks']-data['simulation_elapsed_ms']/20)<4,data
    assert data['minimum_shot_interval_ms']==0 or data['minimum_shot_interval_ms']>=135,data
    hashes=[line.split() for line in eyes.read_text().splitlines()]
    assert len(hashes)==110 and sum(left!=right for _,left,right in hashes)>100
    assert len({left for _,left,_ in hashes})>50
    (OUT/'report.json').write_text(json.dumps({'passed':True,'levels':list(range(1,17)),'stereo_frames':len(hashes),'controller_shot_overrides':data['controller_shots'],'use_actions':data['use_actions'],'movement_first_world_xz':data['movement_first_world_xz'],'distinct_left_views':len({left for _,left,_ in hashes}),'physical_headset_tested':False,'renderer':data['renderer']},indent=2))
    print((OUT/'report.json').read_text())
    env['ABXR_TEST_MENU']='1'
    result=subprocess.run([binary,'--xr','--frames','150','--report',OUT/'menu.json','--capture',OUT/'menu.ppm'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'menu.log').write_text(result.stdout+result.stderr)
    menu=json.loads((OUT/'menu.json').read_text())
    assert result.returncode==0 and menu['xr_entries']==1 and menu['xr_suspends']==0,menu
    assert menu['menu_opens']==2 and menu['smooth_turn'] and menu['saves']==1 and menu['loads']==1,menu
    assert .5<menu['virtual_yaw_deg']<25,menu
    assert menu['frames']<100 and menu['menu_selection']==5,menu
    assert menu['height_resets']==1 and menu['height_calibration_y']>1.85,menu
    assert abs(menu['head_height_offset_m'])<.05 and menu['engine_height_m']>1.4,menu
    assert 'save restored' in result.stdout+result.stderr
    assert (Path(directory)/'state/alien-breed-xr/savegame.bin').exists()
    print('PASS live XR pause menu, height/floor reset, turn-mode changes, save, load/reload and quit')
    env['ABXR_TEST_WALL']='1'
    result=subprocess.run([binary,'--xr','--frames','40','--report',OUT/'wall.json'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'wall.log').write_text(result.stdout+result.stderr)
    wall=json.loads((OUT/'wall.json').read_text())
    assert result.returncode==0 and wall['xr_entries']==1 and wall['xr_suspends']==0,wall
    assert wall['wall_blocked_frames']==11 and wall['wall_fade']==0,wall
    print('PASS tracked head obstruction fades both eyes and recovers without restarting XR')
