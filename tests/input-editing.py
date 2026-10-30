#!/usr/bin/env python3
"""Exercise Focus input through real X11 keyboard, pointer and selection events."""
import ctypes
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

repo=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    profile=root/'input.ini'
    profile.write_text('''[ui]
mode=focus
margin=20
[header]
visible=false
[sidebar]
visible=false
[heading]
visible=false
[grid]
visible=false
[footer]
visible=false
[fonts]
search=DejaVuSansMono/16
[search]
allow_commands=true
padding=10
show_shortcut=false
icon=
selected_background=3366ccff
selected_text=ffffffff
''')
    (root/'empty.dsv').write_text('')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp','-r'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
    env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),LANG='C.UTF-8',LC_ALL='C.UTF-8',XDG_CACHE_HOME=str(root/'cache'))
    os.close(read_fd)
    # Inspect the displayed cursor through XFixes, including its theme-assigned name.
    class CursorImage(ctypes.Structure):
        _fields_=[('x',ctypes.c_short),('y',ctypes.c_short),('width',ctypes.c_ushort),('height',ctypes.c_ushort),
                  ('xhot',ctypes.c_ushort),('yhot',ctypes.c_ushort),('serial',ctypes.c_ulong),
                  ('pixels',ctypes.POINTER(ctypes.c_ulong)),('atom',ctypes.c_ulong),('name',ctypes.c_char_p)]
    xlib=ctypes.CDLL('libX11.so.6'); xfixes=ctypes.CDLL('libXfixes.so.3')
    xlib.XOpenDisplay.argtypes=[ctypes.c_char_p]; xlib.XOpenDisplay.restype=ctypes.c_void_p
    xlib.XFree.argtypes=[ctypes.c_void_p]; xlib.XCloseDisplay.argtypes=[ctypes.c_void_p]
    display=xlib.XOpenDisplay(env['DISPLAY'].encode()); assert display
    xfixes.XFixesGetCursorImage.argtypes=[ctypes.c_void_p]; xfixes.XFixesGetCursorImage.restype=ctypes.POINTER(CursorImage)
    imlib=ctypes.CDLL('libImlib2.so.1')
    imlib.imlib_add_path_to_font_path.argtypes=[ctypes.c_char_p]
    imlib.imlib_load_font.argtypes=[ctypes.c_char_p]; imlib.imlib_load_font.restype=ctypes.c_void_p
    imlib.imlib_context_set_font.argtypes=[ctypes.c_void_p]
    imlib.imlib_get_text_size.argtypes=[ctypes.c_char_p,ctypes.POINTER(ctypes.c_int),ctypes.POINTER(ctypes.c_int)]
    imlib.imlib_add_path_to_font_path(b'/usr/share/fonts/truetype/dejavu')
    font=imlib.imlib_load_font(b'DejaVuSansMono/16'); assert font
    imlib.imlib_context_set_font(font)
    text_x=20+10+imlib.imlib_get_maximum_font_ascent()-imlib.imlib_get_maximum_font_descent()+12

    # Send physical X events; each step waits for editing and clipboard round trips.
    def send(*arguments):
        subprocess.run(['xdotool',*map(str,arguments)],env=env,check=True,stdout=subprocess.DEVNULL)
        time.sleep(.08)

    # Place the pointer on a UTF-8 prefix boundary in the actual configured font.
    def pointer(prefix):
        width=ctypes.c_int(); height=ctypes.c_int()
        imlib.imlib_get_text_size(prefix.encode(),ctypes.byref(width),ctypes.byref(height))
        send('mousemove',text_x+width.value,50)
        cursor=xfixes.XFixesGetCursorImage(display)
        name=cursor.contents.name
        xlib.XFree(cursor)
        assert name==b'text',('Expected themed text cursor',name)

    # Return only the accepted command; --outputonly prevents command execution.
    def case(name,text,actions,expected):
        process=subprocess.Popen([str(repo/'xlunch'),'--ui-config',str(profile),'--outputonly','-i',str(root/'empty.dsv')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            for attempt in range(100):
                found=subprocess.run(['xdotool','search','--onlyvisible','--class','xlunch'],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
                if found.stdout.strip(): break
                assert process.poll() is None,process.communicate()
                time.sleep(.05)
            else: raise AssertionError('Launcher window did not appear')
            send('windowfocus',found.stdout.splitlines()[0])
            send('type','--clearmodifiers','--delay','20','--',text)
            actions()
            send('key','Return')
            output,errors=process.communicate(timeout=5)
            assert process.returncode==0,(name,process.returncode,errors)
            assert output.rstrip('\n')==expected,(name,expected,output,errors)
            print('PASS:',name)
        finally:
            if process.poll() is None:
                process.terminate(); process.wait(timeout=5)

    try:
        case('Ctrl+Backspace removes the preceding word','>alpha beta',lambda:send('key','ctrl+BackSpace'),'alpha ')
        case('Word deletion keeps the text after the caret','>alpha beta gamma',lambda:send('key','ctrl+Left','ctrl+BackSpace'),'alpha gamma')
        case('Unicode word deletion preserves complete characters','>žluťoučký 世界',lambda:send('key','ctrl+BackSpace'),'žluťoučký ')
        case('Shift+Left replaces only the selected characters','>alpha beta',lambda:(send('key','shift+Left','shift+Left'),send('type','XY')),'alpha beXY')
        case('Ctrl+Shift+Left selects a whole word','>alpha beta',lambda:(send('key','ctrl+shift+Left'),send('type','gamma')),'alpha gamma')
        case('Shift+Home selection deletes only the selected prefix','>alpha beta',lambda:send('key','Home','Right','shift+End','BackSpace'),'')
        case('Arrow collapses the selection at its edge','>alpha beta',lambda:(send('key','ctrl+shift+Left','Left'),send('type','X')),'alpha Xbeta')
        case('Copy, cut and paste preserve UTF-8 selections','>alpha žluťoučký',lambda:(send('key','ctrl+shift+Left','ctrl+x','ctrl+v')),'alpha žluťoučký')
        case('Paste replaces the current selection','>alpha beta',lambda:send('key','ctrl+shift+Left','ctrl+c','Home','Right','ctrl+shift+Right','ctrl+v'),'betabeta')

        # Drag in both directions, Shift-click, and PRIMARY paste use the same range.
        def drag(start,end):
            pointer(start); send('mousedown','1'); pointer(end); send('mouseup','1')
        case('Mouse drag replaces just the selected word','>alpha beta',lambda:(drag('>alpha ','>alpha beta'),send('type','gamma')),'alpha gamma')
        case('Backward mouse drag selects the same range','>alpha beta',lambda:(drag('>alpha beta','>alpha '),send('key','BackSpace')),'alpha ')
        case('Shift-click extends from the existing caret','>alpha beta',lambda:(pointer('>alpha '),send('click','1'),send('keydown','Shift_L'),pointer('>alpha beta'),send('click','1'),send('keyup','Shift_L'),send('type','gamma')),'alpha gamma')
        case('Middle-click pastes the PRIMARY selection at the pointer','>alpha beta',lambda:(drag('>alpha ','>alpha beta'),pointer('>'),send('click','2')),'betaalpha beta')
        case('Selection remains correct when the input scrolls','>'+('word '*30)+'last',lambda:(send('key','ctrl+shift+Left'),send('type','tail')),'word '*30+'tail')
    finally:
        xlib.XCloseDisplay(display)
        server.terminate(); server.wait(timeout=5)
