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
    for key in ('WAYLAND_DISPLAY','WAYLAND_SOCKET','DISPLAY','LD_PRELOAD','ABXR_SAVE_DIR','ABXR_CONFIG','ABXR_TEST_MENU','ABXR_TEST_WALL','ABXR_TEST_TWO_HAND','ABXR_TEST_MUZZLE','ABXR_TEST_TAP','ABXR_TEST_PRESENTATION'):env.pop(key,None)
    # Use isolated, all-original-weapons fixtures without editing the user's INI.
    game=Path(directory)/'game';game.mkdir()
    binary=game/'alien-breed-xr';shutil.copy2(BUILD/'alien-breed-xr',binary)
    (game/'data').symlink_to(BUILD/'data',target_is_directory=True)
    (game/'fonts').symlink_to(BUILD/'fonts',target_is_directory=True)
    config=(ROOT/'ab3d.ini.template').read_text().replace('all_weapons=0','all_weapons=1').replace('flamethrower_weapon=0','flamethrower_weapon=1')
    (game/'ab3d.ini').write_text(config)
    content_audit=[]
    expected_pickups=[20,27,48,39,31,43,70,82,43,56,61,67,51,100,48,102]
    expected_enemies=[26,40,38,48,50,37,70,48,38,52,90,73,44,56,80,24]
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
        content=data['original_content']
        assert not content.get('error') and content['placement_changes']==0 and content['unrenderable']==0,content
        assert content['pickups']==expected_pickups[level-1] and content['enemies']==expected_enemies[level-1],content
        assert data['lighting_palette_uploads']>=10 and data['lighting_point_updates']>=1,data
        content_audit.append({'level':level,**content})
        assert data['vector_objects_rendered']>0 and data['vector_meshes_uploaded']>0,data
        assert data['switch_mount_errors']==0,data
        assert data['switch_panels_mounted']==content['switches'],data
        if level==1:
            assert data['switch_panels_mounted']==6 and data['switch_panels_rendered']>=6*12,data
    (OUT/'content-audit.json').write_text(json.dumps(content_audit,indent=2))
    eyes=OUT/'eyes.txt';eyes.unlink(missing_ok=True)
    env.update(LD_PRELOAD=str(BUILD/'libab3d-mock-openxr.so'),ABXR_TEST_EYES=str(eyes))
    env['ABXR_TEST_PRESENTATION']='1'
    env['ABXR_TEST_EYES']=str(OUT/'presentation-eyes.txt')
    result=subprocess.run([binary,'--xr','--frames','110','--presentation','--report',OUT/'presentation.json'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'presentation.log').write_text(result.stdout+result.stderr)
    pages=json.loads((OUT/'presentation.json').read_text())
    assert result.returncode==0 and pages['presentation']==0,pages
    assert pages['title_frames']>=30 and pages['story_frames']>=30,pages
    assert pages['actual_shots_fired']==0 and pages['use_actions']==0 and pages['weapon_changes']==0,pages
    assert pages['movement_samples']==0 and 0<pages['weapon_draws']<100,pages
    assert pages['simulation_vblanks']<45,pages
    print('PASS original stereo title/story, frozen gameplay, fresh-action dismissal and no carried trigger/Use')
    env.pop('ABXR_TEST_PRESENTATION')
    env['ABXR_TEST_EYES']=str(eyes)
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
    assert data['head_pushback_frames']==0 and data['head_pushback_m']<.0001,data
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
    env['ABXR_TEST_TWO_HAND']='1'
    env.pop('ABXR_TEST_EYES',None)
    result=subprocess.run([binary,'--xr','--frames','110','--report',OUT/'two-hand.json'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'two-hand.log').write_text(result.stdout+result.stderr)
    support=json.loads((OUT/'two-hand.json').read_text())
    assert result.returncode==0 and support['two_hand_enabled'] and not support['two_hand_active'],support
    assert support['two_hand_grabs']==3 and support['two_hand_releases']==3 and support['two_hand_frames']==35,support
    assert support['use_actions']==0 and support['weapon_changes']==0 and support['actual_shots_fired']>0,support
    assert support['minimum_shot_interval_ms']>=135,support
    print('PASS two-hand grip, physical aiming, tracking-loss/out-of-reach release and unchanged Use/fire cadence')
    env.pop('ABXR_TEST_TWO_HAND')
    env['ABXR_TEST_MUZZLE']='1'
    result=subprocess.run([binary,'--xr','--frames','110','--report',OUT/'muzzle.json'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'muzzle.log').write_text(result.stdout+result.stderr)
    muzzle=json.loads((OUT/'muzzle.json').read_text())
    assert result.returncode==0 and muzzle['muzzle_retractions']>5 and muzzle['actual_shots_fired']>=4,muzzle
    assert muzzle['minimum_shot_interval_ms']>=135 and muzzle['weapon_changes']==0,muzzle
    print('PASS downward floor-obstructed muzzle keeps firing at original cadence')
    env.pop('ABXR_TEST_MUZZLE')
    env['ABXR_TEST_TAP']='1'
    result=subprocess.run([binary,'--xr','--frames','110','--report',OUT/'tap.json'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'tap.log').write_text(result.stdout+result.stderr)
    tap=json.loads((OUT/'tap.json').read_text())
    assert result.returncode==0 and tap['actual_shots_fired']==2,tap
    print('PASS two short XR trigger taps survive simulation/weapon tick quantization without autofire')
    env.pop('ABXR_TEST_TAP')
    env['ABXR_TEST_EYES']=str(eyes)
    env['ABXR_TEST_MENU']='1'
    result=subprocess.run([binary,'--xr','--frames','150','--report',OUT/'menu.json','--capture',OUT/'menu.ppm'],env=env,capture_output=True,text=True,timeout=35)
    (OUT/'menu.log').write_text(result.stdout+result.stderr)
    menu=json.loads((OUT/'menu.json').read_text())
    assert result.returncode==0 and menu['xr_entries']==1 and menu['xr_suspends']==0,menu
    assert menu['menu_opens']==2 and menu['smooth_turn'] and menu['saves']==1 and menu['loads']==1,menu
    assert .5<menu['virtual_yaw_deg']<25,menu
    assert menu['frames']<115 and menu['menu_selection']==6,menu
    assert menu['two_hand_enabled'] and menu['two_hand_changes']==2,menu
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
    assert wall['head_pushback_frames']==11 and wall['head_pushback_m']<.0001 and wall['maximum_head_pushback_m']>1,wall
    print('PASS tracked head pushback keeps the view clear and recovers without restarting XR')
