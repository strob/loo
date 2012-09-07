from numpy import *

loops = []
recording = False
cur_recording = []
cur_recording_video = []

def video_out(a):
    if len(loops)==0:
        return
    l_h = 240/len(loops)
    for i,loop in enumerate(loops):
        y1 = i*l_h
        y2 = (i+1)*l_h
        col = ((i+1) % 2) * 255

        v_idx = (loop['idx']*len(loop['vid'])) / len(loop['np'])
        a[y1:y2] = loop['vid'][v_idx][y1:y2]

        x = 320* (loop['idx'] / float(len(loop['np'])))
        a[y1:y2,x] = (255,255-col,0)

def video_in(a):
    if recording:
        cur_recording_video.append(a)

def audio_out(a):
    for loop in loops:
        a[:] += rollsome(loop['np'], loop['idx'], len(a)) / len(loops)
        loop['idx'] = (loop['idx'] + len(a)) % (len(loop['np']))

def audio_in(a):
    if recording:
        cur_recording.append(a)

def keyboard_in(type, key):
    global recording, cur_recording, cur_recording_video, loops
    if type=='key-press':
        if key=='space':
            print 'toggle recording'
            recording = not recording
            if not recording:
                loop = {'idx': 0,
                        'vid': cur_recording_video,
                        'np': concatenate(cur_recording)}

                loops.append(loop)
                cur_recording = []
                cur_recording_video = []
        elif key=='r':
            loops = []
        elif key=='s':
            import os, glob
            for f in glob.glob('last/*'):
                os.remove(f)
            for i,l in enumerate(loops):
                save('last/%02d-audio.npy' % (i), l['np'])
                save('last/%02d-video.npy' % (i), l['vid'])
            print 'saved'
        elif key=='l':
            import glob
            loops = []
            for f in glob.glob('last/*audio*'):
                loops.append({'np': load(f),
                              'vid': load(f.replace('audio', 'video')),
                              'idx': 0})

def mouse_in(type,px,py,button):
    l_num = int(py * len(loops))
    if len(loops)==0:
        return
    if type=='mouse-move':
        loops[l_num]['idx'] = int(len(loops[l_num]['np'])*px)
    elif type=='mouse-button-press':
        del loops[l_num]

def rollsome(arr, idx, N):
    "like numpy.roll, but only returns the first N entries"
    if len(arr)-idx >= N:
        return arr[idx:idx+N]
    else:
        return concatenate([arr[idx:], arr[:N-(len(arr)-idx)]])
