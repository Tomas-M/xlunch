#!/usr/bin/env python3
"""Keep both launcher interfaces usable after their desktop wallpaper disappears."""
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
xlib.XInternAtom.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int]; xlib.XInternAtom.restype=ctypes.c_ulong
xlib.XCreatePixmap.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint]; xlib.XCreatePixmap.restype=ctypes.c_ulong
xlib.XFreePixmap.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
xlib.XChangeProperty.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_void_p,ctypes.c_int]
xlib.XDeleteProperty.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong]
xlib.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
xlib.XCloseDisplay.argtypes=[ctypes.c_void_p]

with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    (root/'empty.dsv').write_text('')
    (root/'focus.ini').write_text('[ui]\nmode=focus\n[background]\nuse_root=true\n')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    display=None
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),LANG='C.UTF-8',LC_ALL='C.UTF-8',XDG_CACHE_HOME=str(root/'cache'))
        display=xlib.XOpenDisplay(env['DISPLAY'].encode()); assert display
        window=xlib.XDefaultRootWindow(display)
        atom=xlib.XInternAtom(display,b'_XROOTPMAP_ID',False)
        for mode in (['--rootwindowbackground'],['--ui-config',str(root/'focus.ini')]):
            for initial in ('missing','valid','destroyed'):
                pixmap=xlib.XCreatePixmap(display,window,48,32,24)
                value=ctypes.c_ulong(pixmap)
                xlib.XChangeProperty(display,window,atom,20,32,0,ctypes.byref(value),1)
                if initial=='missing': xlib.XDeleteProperty(display,window,atom)
                if initial=='destroyed': xlib.XFreePixmap(display,pixmap)
                xlib.XSync(display,False)
                process=subprocess.Popen([str(repo/'xlunch'),'--multiple','--font','DejaVuSans/10','--input',str(root/'empty.dsv'),*mode],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
                try:
                    time.sleep(.4)
                    assert process.poll() is None,(mode,initial,process.stderr.read().decode())
                    # Replace the wallpaper, then destroy it and announce its stale ID again.
                    if initial!='destroyed': xlib.XFreePixmap(display,pixmap)
                    pixmap=xlib.XCreatePixmap(display,window,800,600,24)
                    value=ctypes.c_ulong(pixmap)
                    xlib.XChangeProperty(display,window,atom,20,32,0,ctypes.byref(value),1)
                    xlib.XSync(display,False)
                    time.sleep(.2)
                    xlib.XFreePixmap(display,pixmap)
                    xlib.XChangeProperty(display,window,atom,20,32,0,ctypes.byref(value),1)
                    xlib.XSync(display,False)
                    time.sleep(.2)
                    assert process.poll() is None,(mode,initial,process.stderr.read().decode())
                    # Confirm keyboard input still reaches the launcher after the failed read.
                    subprocess.run(['xdotool','key','Escape'],env=env,check=True)
                    output,error=process.communicate(timeout=5)
                    assert process.returncode==0x20,(process.returncode,error.decode())
                    assert not error,error.decode()
                    print('PASS:',mode[0],initial,'startup, replacement, destruction and Escape')
                finally:
                    if process.poll() is None: process.terminate()
                    process.communicate(timeout=5)
    finally:
        if display: xlib.XCloseDisplay(display)
        os.close(read_fd)
        server.terminate(); server.wait(timeout=5)
