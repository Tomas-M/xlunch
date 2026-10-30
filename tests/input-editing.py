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
show_shortcut=true
icon=
background=223344ff
hover_background=445566ff
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
    xtest=ctypes.CDLL('libXtst.so.6')
    xtest.XTestFakeMotionEvent.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_ulong]
    class Crossing(ctypes.Structure):
        _fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send_event',ctypes.c_int),('display',ctypes.c_void_p),
                  ('window',ctypes.c_ulong),('root',ctypes.c_ulong),('subwindow',ctypes.c_ulong),('time',ctypes.c_ulong),
                  ('x',ctypes.c_int),('y',ctypes.c_int),('x_root',ctypes.c_int),('y_root',ctypes.c_int),
                  ('mode',ctypes.c_int),('detail',ctypes.c_int),('same_screen',ctypes.c_int),('focus',ctypes.c_int),('state',ctypes.c_uint)]
    class Event(ctypes.Union):
        _fields_=[('crossing',Crossing),('pad',ctypes.c_long*24)]
    xlib.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
    xlib.XFlush.argtypes=[ctypes.c_void_p]
    xlib.XGetImage.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_int]
    xlib.XGetImage.restype=ctypes.c_void_p
    xlib.XGetPixel.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_int]; xlib.XGetPixel.restype=ctypes.c_ulong
    xlib.XDestroyImage.argtypes=[ctypes.c_void_p]
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

    # Query the actual displayed cursor rather than the window's requested state.
    def cursor_name():
        cursor=xfixes.XFixesGetCursorImage(display)
        name=cursor.contents.name
        xlib.XFree(cursor)
        return name

    # Place the pointer on a UTF-8 prefix boundary in the actual configured font.
    def pointer(prefix):
        width=ctypes.c_int(); height=ctypes.c_int()
        imlib.imlib_get_text_size(prefix.encode(),ctypes.byref(width),ctypes.byref(height))
        send('mousemove',text_x+width.value,50)
        name=cursor_name()
        assert name==b'text',('Expected themed text cursor',name)

    # Replay grab crossings with time to render between them, checking both cursor and hover pixels.
    def grab_crossings():
        pointer('>alpha ')
        window=int(subprocess.check_output(['xdotool','getwindowfocus'],env=env,text=True))
        image=xlib.XGetImage(display,window,700,50,1,1,ctypes.c_ulong(-1).value,2)
        background=xlib.XGetPixel(image,0,0); xlib.XDestroyImage(image)
        for event_type,mode,mask in [(8,1,1<<5),(7,2,1<<4)]:
            event=Event(); event.crossing=Crossing(type=event_type,display=display,window=window,x=100,y=50,mode=mode,same_screen=1)
            xlib.XSendEvent(display,window,False,mask,ctypes.byref(event)); xlib.XFlush(display)
            time.sleep(.1)
            assert cursor_name()==b'text','Pointer grab replaced the text cursor'
            image=xlib.XGetImage(display,window,700,50,1,1,ctypes.c_ulong(-1).value,2)
            actual=xlib.XGetPixel(image,0,0); xlib.XDestroyImage(image)
            assert actual==background,'Pointer grab changed the input hover background'

    # Search icons and shortcut hints focus the input without editing it.
    def outside_text():
        send('key','ctrl+shift+Left')
        for x in (40,740):
            send('mousemove',x,50)
            arrow=cursor_name()
            assert arrow!=b'text','Non-editable search chrome has a text cursor'
            send('mousedown','1')
            assert cursor_name()==arrow,'Clicking outside the text viewport starts a text drag'
            send('mouseup','1')
            assert cursor_name()==arrow,'Releasing outside the text viewport changes the cursor'
        send('type','gamma')

    # A selection started in the text still extends past the viewport until release.
    def drag_outside_text():
        pointer('>alpha '); send('mousedown','1'); send('mousemove',40,50)
        assert cursor_name()==b'text','Active text drag lost its cursor outside the viewport'
        send('mouseup','1')
        assert cursor_name()!=b'text','Released text drag kept its cursor over the icon'
        send('key','End')

    # Read the insertion-point pixel without including the mouse cursor overlay.
    def caret_visible(window,prefix):
        width=ctypes.c_int(); height=ctypes.c_int()
        imlib.imlib_get_text_size(prefix.encode(),ctypes.byref(width),ctypes.byref(height))
        image=xlib.XGetImage(display,window,text_x+width.value,50,1,1,ctypes.c_ulong(-1).value,2)
        pixel=xlib.XGetPixel(image,0,0); xlib.XDestroyImage(image)
        return pixel&0xffffff==0xffffff

    # Check actual on-screen phase transitions rather than only a timer calculation.
    def blinking_caret():
        window=int(subprocess.check_output(['xdotool','getwindowfocus'],env=env,text=True))
        time.sleep(1); send('key','End')
        started=time.monotonic(); previous=caret_visible(window,'>alpha beta'); changes=[]
        assert previous,'Caret must start visible after typing'
        while time.monotonic()-started<1.7:
            visible=caret_visible(window,'>alpha beta')
            if visible!=previous:
                changes.append(time.monotonic()); previous=visible
            time.sleep(.015)
        assert len(changes)>=3,('Caret did not alternate at 500 ms',changes)
        intervals=[end-start for start,end in zip(changes,changes[1:])]
        assert all(.35<interval<.65 for interval in intervals),intervals

    # Typing keeps the caret visible; idle pointer motion must not restart a hidden phase.
    def typing_caret():
        window=int(subprocess.check_output(['xdotool','getwindowfocus'],env=env,text=True))
        prefix='>alpha'
        for character in 'bcde':
            send('type',character); prefix+=character
            assert caret_visible(window,prefix),'Caret disappeared while typing'
            time.sleep(.12)
        time.sleep(.5)
        assert not caret_visible(window,prefix),'Caret did not hide after typing stopped'
        xtest.XTestFakeMotionEvent(display,-1,500,50,0); xlib.XFlush(display)
        time.sleep(.025)
        assert not caret_visible(window,prefix),'Ordinary pointer motion reset caret blinking'
        send('key','Left')
        assert caret_visible(window,prefix[:-1]),'Moving the caret did not make it visible'

    # A nonempty selection hides the caret through every blink phase until collapsed.
    def selected_caret():
        window=int(subprocess.check_output(['xdotool','getwindowfocus'],env=env,text=True))
        send('key','ctrl+a')
        assert not caret_visible(window,'>alpha beta'),'Selection left the caret visible'
        time.sleep(.6)
        send('mousemove',500,50)
        assert not caret_visible(window,'>alpha beta'),'Caret blinked while text was selected'
        send('key','End')
        assert caret_visible(window,'>alpha beta'),'Collapsed selection did not restore the caret'

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
        case('Caret blinks with 500 ms phases','>alpha beta',blinking_caret,'alpha beta')
        case('Typing and caret movement reset blinking; pointer motion does not','>alpha',typing_caret,'alphabcde')
        case('Caret stays hidden while text is selected','>alpha beta',selected_caret,'alpha beta')
        case('Icon and shortcut clicks preserve arrow and selection','>alpha beta',outside_text,'alpha gamma')
        case('Text drag retains its cursor only until release','>alpha beta',drag_outside_text,'alpha beta')
        case('Pointer grabs preserve the text cursor and input background','>alpha beta',grab_crossings,'alpha beta')
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
