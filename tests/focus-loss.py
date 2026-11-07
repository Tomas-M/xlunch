#!/usr/bin/env python3
"""Distinguish real focus changes from temporary keyboard grabs and child focus."""
import ctypes
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

repo=Path(__file__).resolve().parents[1]
xlib=ctypes.CDLL('libX11.so.6')
xlib.XOpenDisplay.argtypes=[ctypes.c_char_p]; xlib.XOpenDisplay.restype=ctypes.c_void_p
xlib.XDefaultRootWindow.argtypes=[ctypes.c_void_p]; xlib.XDefaultRootWindow.restype=ctypes.c_ulong
xlib.XCreateSimpleWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_ulong]; xlib.XCreateSimpleWindow.restype=ctypes.c_ulong
xlib.XMapWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
xlib.XDestroyWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
xlib.XSetInputFocus.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong]
xlib.XGrabKeyboard.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_ulong]
xlib.XUngrabKeyboard.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
xlib.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
xlib.XCloseDisplay.argtypes=[ctypes.c_void_p]

with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    (root/'empty.dsv').write_text('')
    (root/'focus.ini').write_text('[ui]\nmode=focus\n')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    display=None
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),LANG='C.UTF-8',LC_ALL='C.UTF-8',XDG_CACHE_HOME=str(root/'cache'))
        display=xlib.XOpenDisplay(env['DISPLAY'].encode()); assert display
        target=xlib.XCreateSimpleWindow(display,xlib.XDefaultRootWindow(display),0,0,100,100,0,0,0)
        xlib.XMapWindow(display,target); xlib.XSync(display,False)
        for mode in ([],['--ui-config',str(root/'focus.ini')]):
            for terminate,grabbed in ((False,False),(True,False),(True,True)):
                options=['--focuslostterminate'] if terminate else []
                process=subprocess.Popen([str(repo/'xlunch'),'--multiple','--font','DejaVuSans/10','--input',str(root/'empty.dsv'),*mode,*options],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
                try:
                    deadline=time.monotonic()+5
                    while True:
                        result=subprocess.run(['xdotool','search','--onlyvisible','--class','^xlunch-fullscreen$'],env=env,capture_output=True,text=True)
                        if result.returncode==0: break
                        assert process.poll() is None,process.stderr.read().decode()
                        assert time.monotonic()<deadline,'Launcher did not map'
                        time.sleep(.02)
                    window=int(result.stdout.splitlines()[0])
                    xlib.XSetInputFocus(display,window,2,0); xlib.XSync(display,False)
                    time.sleep(.1)
                    assert xlib.XGrabKeyboard(display,target,False,1,1,0)==0
                    xlib.XSync(display,False); time.sleep(.1)
                    assert process.poll() is None,'A keyboard grab closed the launcher'
                    xlib.XUngrabKeyboard(display,0); xlib.XSync(display,False)
                    child=xlib.XCreateSimpleWindow(display,window,0,0,20,20,0,0,0)
                    xlib.XMapWindow(display,child)
                    xlib.XSetInputFocus(display,child,2,0); xlib.XSync(display,False)
                    time.sleep(.1)
                    assert process.poll() is None,'Child focus closed the launcher'
                    xlib.XSetInputFocus(display,window,2,0)
                    xlib.XDestroyWindow(display,child)
                    if grabbed: assert xlib.XGrabKeyboard(display,target,False,1,1,0)==0
                    xlib.XSetInputFocus(display,target,2,0); xlib.XSync(display,False)
                    if terminate:
                        output,error=process.communicate(timeout=5)
                        assert process.returncode==0x23,(process.returncode,error.decode())
                        assert not error,error.decode()
                    else:
                        time.sleep(.1)
                        assert process.poll() is None,'Default focus-loss behavior changed'
                    print('PASS:',mode or 'classic','terminate='+str(terminate),'while-grabbed='+str(grabbed))
                finally:
                    xlib.XUngrabKeyboard(display,0); xlib.XSync(display,False)
                    if process.poll() is None: process.terminate()
                    process.communicate(timeout=5)
    finally:
        if display: xlib.XCloseDisplay(display)
        os.close(read_fd)
        server.terminate(); server.wait(timeout=5)
