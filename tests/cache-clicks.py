#!/usr/bin/env python3
"""Keep clicks stable across real background cache refreshes and one-second reorders."""
import contextlib
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

repo=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='xlunch-cache-clicks-') as directory:
    root=Path(directory)
    for path in ('home','data/applications','cache','empty'):
        (root/path).mkdir(parents=True)
    icon=root/'icon.svg'
    icon.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" fill="#369"/></svg>')
    profile=root/'focus.ini'
    profile.write_text('''[ui]
mode=focus
margin=20
padding=0
[entries]
source=desktop
[header]
visible=false
[sidebar]
visible=false
[heading]
visible=false
[search]
visible=false
[footer]
visible=false
[session]
visible=false
[scrollbar]
visible=false
[grid]
columns=3
tile_min_width=200
tile_min_height=140
icon_size=64
padding=12
gap_x=20
gap_y=20
label_lines=1
''')
    # Stop the real worker before it reads desktop files; leave xlunch itself unmodified.
    shim=root/'pause.c'
    shim.write_text('''#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
pid_t fork(void)
{
    pid_t (*real_fork)(void)=dlsym(RTLD_NEXT,"fork");
    pid_t child=real_fork();
    if (!child) raise(SIGSTOP);
    return child;
}
''')
    subprocess.run(['cc','-shared','-fPIC','-o',str(root/'pause.so'),str(shim),'-ldl'],check=True)
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env=dict(os.environ,DISPLAY=':'+os.read(read_fd,32).decode().strip(),HOME=str(root/'home'),LANG='C.UTF-8',LC_ALL='C.UTF-8',XDG_DATA_HOME=str(root/'data'),XDG_DATA_DIRS=str(root/'empty'),XDG_CACHE_HOME=str(root/'cache'))
        args=[str(repo/'xlunch'),'--ui-config',str(profile),'--multiple','--outputonly']

        # Recreate a controlled catalog with stable desktop IDs and alphabetical names.
        def catalog(names):
            for path in (root/'data/applications').glob('*.desktop'): path.unlink()
            for name in names:
                (root/f'data/applications/{name}.desktop').write_text(f'[Desktop Entry]\nType=Application\nName={name}\nExec=true\nIcon={icon}\n')

        # Deliver real X11 input instead of calling implementation functions directly.
        def send(*arguments):
            subprocess.run(['xdotool',*map(str,arguments)],env=env,check=True,stdout=subprocess.DEVNULL)

        # Run every scenario from a valid old cache with a paused refresh worker.
        @contextlib.contextmanager
        def launcher(names):
            catalog(names)
            subprocess.run(args+['--rebuild-cache'],env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
            process=subprocess.Popen(args,env=dict(env,LD_PRELOAD=str(root/'pause.so')),stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            worker=None
            try:
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    assert process.poll() is None,'Launcher exited before the first frame'
                    children=Path(f'/proc/{process.pid}/task/{process.pid}/children').read_text().split()
                    if children:
                        worker=int(children[0])
                        if '\nState:\tT' in Path(f'/proc/{worker}/status').read_text(): break
                    time.sleep(.005)
                assert worker is not None,'No background refresh worker'
                yield process,worker
            finally:
                send('mouseup',1)
                if worker is not None:
                    try: os.kill(worker,signal.SIGCONT)
                    except ProcessLookupError: pass
                if process.poll() is None:
                    process.terminate(); process.communicate(timeout=5)

        # Finish the worker and allow the parent to consume its completed snapshot.
        def refresh(process,worker,names=None):
            if names is not None: catalog(names)
            os.kill(worker,signal.SIGCONT)
            deadline=time.monotonic()+5
            while Path(f'/proc/{worker}').exists() and time.monotonic()<deadline: time.sleep(.005)
            assert not Path(f'/proc/{worker}').exists(),'Worker was not reaped'
            time.sleep(.06)
            assert process.poll() is None,'Launcher exited during refresh'

        # Assert the public output-only selection without launching test applications.
        def selected(process,name):
            output,error=process.communicate(timeout=3)
            assert process.returncode==0 and output.decode().strip()==str(root/f'data/applications/{name}.desktop'),(output,error)

        # A click that was blocked on press must remain blocked even after a long hold.
        def blocked(process,x,y,hold=0):
            send('mousemove',x,y,'mousedown',1)
            time.sleep(hold)
            send('mouseup',1)
            time.sleep(.06)
            assert process.poll() is None,'A recently moved application was launched'

        for changed in (False,True):
            with launcher(['Alpha','Charlie']) as (process,worker):
                cache=next((root/'cache/xlunch').iterdir())
                before=cache.read_bytes()
                send('mousemove',400,80,'mousedown',1)
                time.sleep(.03)
                refresh(process,worker,['Alpha','Bravo','Charlie'] if changed else None)
                if not changed: assert cache.read_bytes()==before
                ticks=lambda:sum(map(int,Path(f'/proc/{process.pid}/stat').read_text().split()[13:15]))
                start=ticks(); time.sleep(.25)
                assert ticks()-start<os.sysconf('SC_CLK_TCK')/5,'Pending refresh spins while the button is held'
                send('mouseup',1)
                selected(process,'Charlie')
            print('PASS: click spans', 'reordered' if changed else 'identical', 'cache refresh',flush=True)

        with launcher(['Alpha','Charlie']) as (process,worker):
            refresh(process,worker,['Alpha','Bravo','Charlie'])
            send('mousemove',100,80,'click',1)
            selected(process,'Alpha')
        print('PASS: unchanged first tile remains immediately clickable',flush=True)

        with launcher(['Alpha','Bravo','Delta']) as (process,worker):
            refresh(process,worker,['Alpha','Charlie','Delta'])
            send('mousemove',650,80,'click',1)
            selected(process,'Delta')
        print('PASS: unchanged tile after a changed slot remains clickable',flush=True)

        for label,point,name,hold in [('replacement',(400,80),'Bravo',0),('new slot',(650,80),'Charlie',0),('long hold',(400,80),'Bravo',1.15)]:
            with launcher(['Alpha','Charlie']) as (process,worker):
                refresh(process,worker,['Alpha','Bravo','Charlie'])
                blocked(process,*point,hold)
                if not hold: time.sleep(1.1)
                send('click',1)
                selected(process,name)
            print('PASS:',label,'blocked within grace period, allowed afterward',flush=True)
        profile.write_text(profile.read_text().replace('mode=focus','mode=classic'))
        args+=['--border','20','--sideborder','20','--iconsize','64','--iconpadding','20','--iconvpadding','20','--rows','2','--columns','3','--font','DejaVuSans/10']
        with launcher(['Alpha','Charlie']) as (process,worker):
            send('mousemove',400,80,'mousedown',1)
            refresh(process,worker)
            send('mouseup',1)
            selected(process,'Charlie')
        print('PASS: classic interface preserves a click across refresh',flush=True)
        with launcher(['Alpha','Charlie']) as (process,worker):
            refresh(process,worker,['Alpha','Bravo','Charlie'])
            blocked(process,400,80)
            time.sleep(1.1)
            send('click',1)
            selected(process,'Bravo')
        print('PASS: classic interface guards a replacement tile',flush=True)
    finally:
        os.close(read_fd)
        server.terminate(); server.wait(timeout=5)
