#!/usr/bin/env python3
"""Check command detection and application activation through real X11 input."""
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

repo=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='xlunch-command-routing-') as directory:
    root=Path(directory)
    for path in ('bin','data/applications','empty','home','cache'):
        (root/path).mkdir(parents=True)
    for name in ('X','Xorg','xarchiver'):
        program=root/'bin'/name
        program.write_text('#!/bin/sh\nexit 0\n')
        program.chmod(0o755)
    desktop=root/'data/applications/xarchiver.desktop'
    desktop.write_text('[Desktop Entry]\nType=Application\nName=X Archiver\nExec=xarchiver\n')
    profile=root/'focus.ini'
    profile.write_text('''[ui]
mode=focus
[entries]
source=desktop
[header]
visible=false
[sidebar]
visible=false
[heading]
visible=false
[footer]
visible=false
[search]
allow_commands=true
command_prefix=
''')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env=dict(os.environ,HOME=str(root/'home'),DISPLAY=':'+os.read(read_fd,32).decode().strip(),PATH=str(root/'bin')+':'+os.environ['PATH'],XDG_DATA_HOME=str(root/'data'),XDG_DATA_DIRS=str(root/'empty'),XDG_CACHE_HOME=str(root/'cache'),LANG='C.UTF-8',LC_ALL='C.UTF-8')
        args=[str(repo/'xlunch'),'--ui-config',str(profile),'--multiple','--outputonly']
        subprocess.run(args+['--rebuild-cache'],env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)

        # Output-only mode verifies routing without ever starting an X server.
        def case(query,expected):
            process=subprocess.Popen(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                for attempt in range(100):
                    found=subprocess.run(['xdotool','search','--onlyvisible','--class','xlunch'],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
                    if found.stdout.strip():break
                    assert process.poll() is None,process.communicate()
                    time.sleep(.05)
                else:raise AssertionError('Launcher window did not appear')
                subprocess.run(['xdotool','windowfocus',found.stdout.splitlines()[0]],env=env,check=True)
                subprocess.run(['xdotool','type','--clearmodifiers','--delay','20','--',query],env=env,check=True)
                time.sleep(.15)
                subprocess.run(['xdotool','key','Return'],env=env,check=True)
                if expected is None:
                    time.sleep(.3)
                    assert process.poll() is None,'Excluded X command was accepted without a matching app'
                    subprocess.run(['xdotool','key','Escape','Escape'],env=env,check=True)
                output,errors=process.communicate(timeout=5)
                assert process.returncode==(32 if expected is None else 0),(query,process.returncode,errors)
                assert output.rstrip('\n')==(expected or ''),(query,expected,output,errors)
                print('PASS:',query)
            finally:
                if process.poll() is None:process.terminate();process.communicate(timeout=5)

        case('X Archiver',str(desktop))
        case('X',str(desktop))
        case('"X" Archiver',None)
        case('X no matching application',None)
        case('xarchiver',str(desktop))
        case('true','true')
        case('Xorg --help','Xorg --help')
    finally:
        os.close(read_fd)
        server.terminate();server.wait(timeout=5)
