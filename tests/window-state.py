#!/usr/bin/env python3
"""Inspect initial EWMH state while the first map request is still blocked by a WM."""
import ctypes
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

repo=Path(__file__).resolve().parents[1]

class MapRequest(ctypes.Structure):
    _fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send_event',ctypes.c_int),
              ('display',ctypes.c_void_p),('parent',ctypes.c_ulong),('window',ctypes.c_ulong)]

class Event(ctypes.Union):
    _fields_=[('map',MapRequest),('pad',ctypes.c_long*24)]

with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    (root/'empty.dsv').write_text('')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    xlib=ctypes.CDLL('libX11.so.6')
    xlib.XOpenDisplay.argtypes=[ctypes.c_char_p]; xlib.XOpenDisplay.restype=ctypes.c_void_p
    xlib.XDefaultRootWindow.argtypes=[ctypes.c_void_p]; xlib.XDefaultRootWindow.restype=ctypes.c_ulong
    xlib.XSelectInput.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_long]
    xlib.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
    xlib.XPending.argtypes=[ctypes.c_void_p]
    xlib.XNextEvent.argtypes=[ctypes.c_void_p,ctypes.POINTER(Event)]
    xlib.XCloseDisplay.argtypes=[ctypes.c_void_p]
    xlib.XInternAtom.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int]; xlib.XInternAtom.restype=ctypes.c_ulong
    xlib.XFree.argtypes=[ctypes.c_void_p]
    xlib.XGetWindowProperty.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_long,ctypes.c_long,ctypes.c_int,ctypes.c_ulong,
                                    ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_ulong),
                                    ctypes.POINTER(ctypes.c_ulong),ctypes.POINTER(ctypes.c_void_p)]
    display=None
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),LANG='C.UTF-8',LC_ALL='C.UTF-8',XDG_CACHE_HOME=str(root/'cache'))
        display=xlib.XOpenDisplay(env['DISPLAY'].encode()); assert display
        # Act as a WM and inspect the window before any MapNotify or client-list update.
        xlib.XSelectInput(display,xlib.XDefaultRootWindow(display),1<<20)
        xlib.XSync(display,False)
        for mode in ([],['--windowed']):
            process=subprocess.Popen([str(repo/'xlunch'),'--multiple','--font','DejaVuSans/10','--input',str(root/'empty.dsv'),*mode],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
            try:
                deadline=time.monotonic()+5
                window=None
                while time.monotonic()<deadline and window is None:
                    assert process.poll() is None,process.stderr.read().decode()
                    while xlib.XPending(display):
                        event=Event(); xlib.XNextEvent(display,ctypes.byref(event))
                        if event.map.type==20: window=event.map.window
                    time.sleep(.01)
                assert window,'No initial map request'
                actual_type=ctypes.c_ulong(); actual_format=ctypes.c_int()
                count=ctypes.c_ulong(); remaining=ctypes.c_ulong(); data=ctypes.c_void_p()
                state_atom=xlib.XInternAtom(display,b'_NET_WM_STATE',False)
                xlib.XGetWindowProperty(display,window,state_atom,0,16,False,4,ctypes.byref(actual_type),ctypes.byref(actual_format),ctypes.byref(count),ctypes.byref(remaining),ctypes.byref(data))
                state=set(ctypes.cast(data,ctypes.POINTER(ctypes.c_ulong))[:count.value]) if count.value else set()
                if data: xlib.XFree(data)
                for name in ('_NET_WM_STATE_FULLSCREEN','_NET_WM_STATE_SKIP_PAGER'):
                    assert (xlib.XInternAtom(display,name.encode(),False) in state)==(not mode),(mode,name,state)
                print('PASS:',mode or 'fullscreen','initial state before mapping')
            finally:
                process.terminate(); process.communicate(timeout=5)
    finally:
        if display: xlib.XCloseDisplay(display)
        os.close(read_fd)
        server.terminate(); server.wait(timeout=5)
