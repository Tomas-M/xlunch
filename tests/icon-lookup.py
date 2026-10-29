#!/usr/bin/env python3
"""Exercise theme fallback, symbolic artwork and XSettings without a desktop session."""
import ctypes
import os
from pathlib import Path
import select
import struct
import subprocess
import tempfile

repo=Path(__file__).resolve().parents[1]
binary=repo/'xlunch'

# Read only the catalog and resolved paths from an actual launcher snapshot.
def snapshot(path):
    data=path.read_bytes()
    assert data[:8]==b'XLCACHE2'
    position=8
    def number():
        nonlocal position
        value=struct.unpack_from('=I',data,position)[0]
        position+=4
        return value
    def text():
        nonlocal position
        length=number()
        if length==0xffffffff: return None
        value=data[position:position+length].decode()
        position+=length
        return value
    entries={}
    for unused in range(number()):
        fields=[text() for field in range(10)]
        entries[fields[3]]=fields[1]
    icons={}
    for unused in range(number()):
        name,path,size=text(),text(),number()
        icons[name]=path
    return entries,icons

# Make minimal indexed themes using artwork decoded by the real SVG loader.
def theme(root,name,icons,parent=''):
    directory=root/'share/icons'/name
    (directory/'scalable/apps').mkdir(parents=True)
    (directory/'index.theme').write_text(f'[Icon Theme]\nName={name}\nInherits={parent}\nDirectories=scalable/apps\n[scalable/apps]\nSize=64\nType=Scalable\nMinSize=1\nMaxSize=256\n')
    for icon in icons:
        (directory/'scalable/apps'/f'{icon}.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" fill="#369"/></svg>')
    return directory/'scalable/apps'

with tempfile.TemporaryDirectory(prefix='xlunch-icons-') as temporary:
    root=Path(temporary)
    env=os.environ.copy()
    env.update(HOME=str(root/'home'),XDG_DATA_HOME=str(root/'data'),XDG_DATA_DIRS=str(root/'share'),XDG_CONFIG_HOME=str(root/'config'),XDG_CONFIG_DIRS=str(root/'system'),XDG_CACHE_HOME=str(root/'cache'),LANG='C.UTF-8',LC_ALL='C.UTF-8')
    env.pop('DISPLAY',None)
    for directory in ['home','data/applications','config/gtk-3.0','system/gtk-3.0']:
        (root/directory).mkdir(parents=True)
    settings=root/'config/gtk-3.0/settings.ini'
    settings.write_text('[Settings]\n gtk-icon-theme-name = "TestSystem" \n')
    (root/'system/gtk-3.0/settings.ini').write_text('[Settings]\ngtk-icon-theme-name=IgnoredSystem\n')
    primary=theme(root,'TestPrimary',['xlunch-priority','xlunch-both-symbolic'],'TestParent')
    system=theme(root,'TestSystem',['xlunch-priority','xlunch-system','xlunch-fallback-symbolic','xlunch-already-symbolic'],'TestParent')
    parent=theme(root,'TestParent',['xlunch-parent'])
    hicolor=theme(root,'hicolor',['xlunch-both'])
    unused=theme(root,'UnrelatedTheme',['xlunch-absent'])
    profile=root/'focus.ini'
    profile.write_text('[ui]\nmode=focus\n[entries]\nsource=desktop\n[icons]\ntheme=TestPrimary\n')
    expected={'xlunch-priority':primary/'xlunch-priority.svg','xlunch-system':system/'xlunch-system.svg','xlunch-parent':parent/'xlunch-parent.svg','xlunch-both':hicolor/'xlunch-both.svg','xlunch-fallback':system/'xlunch-fallback-symbolic.svg','xlunch-already-symbolic':system/'xlunch-already-symbolic.svg','xlunch-absent':None,'xlunch-file':unused/'xlunch-absent.svg'}
    for name in expected:
        icon=str(expected[name]) if name=='xlunch-file' else name
        (root/f'data/applications/{name}.desktop').write_text(f'[Desktop Entry]\nType=Application\nName={name}\nExec=true\nIcon={icon}\n')
    subprocess.run([str(binary),'--ui-config',str(profile),'--rebuild-cache'],env=env,check=True)
    caches=list((root/'cache/xlunch').iterdir())
    assert len(caches)==1
    entries,icons=snapshot(caches[0])
    for name,path in expected.items():
        assert entries[name]==(str(path) if path else ''),(name,entries[name],path)
    assert icons['xlunch-absent'] is None
    settings.write_text('[Settings]\ngtk-icon-theme-name=AnotherTheme\n')
    subprocess.run([str(binary),'--ui-config',str(profile),'--rebuild-cache'],env=env,check=True)
    assert len(list((root/'cache/xlunch').iterdir()))==2,'System theme must affect the cache key'
    print('PASS: primary/inherited/system/hicolor priorities, symbolic names, exact paths, no unrelated themes, cache invalidation')

    probe=root/'theme-probe.c'
    probe.write_text('#include "ui.h"\nDisplay *disp;\nint main(int argc,char **argv) { if (argc>1) disp=XOpenDisplay(NULL); puts(ui_system_icon_theme()); if (disp) XCloseDisplay(disp); return 0; }\n')
    flags=subprocess.check_output(['pkg-config','--cflags','imlib2','x11'],text=True).split()
    subprocess.run(['cc',*flags,'-I'+str(repo/'src'),str(probe),str(repo/'src/icon_theme.c'),'-lX11','-o',str(root/'theme-probe')],check=True)
    settings.write_text('[Settings]\ngtk-icon-theme-name=FileTheme\n')
    assert subprocess.check_output([str(root/'theme-probe')],env=env,text=True).strip()=='FileTheme'
    settings.unlink()
    assert subprocess.check_output([str(root/'theme-probe')],env=env,text=True).strip()=='IgnoredSystem'
    settings.write_text('[Settings]\ngtk-icon-theme-name=FileTheme\n')
    read_fd,write_fd=os.pipe()
    server=subprocess.Popen(['Xvfb','-displayfd',str(write_fd),'-screen','0','800x600x24','-nolisten','tcp'],pass_fds=(write_fd,),stderr=subprocess.DEVNULL)
    os.close(write_fd)
    try:
        assert select.select([read_fd],[],[],5)[0],'Xvfb did not start'
        env['DISPLAY']=':'+os.read(read_fd,32).decode().strip()
        os.close(read_fd)
        x=ctypes.CDLL('libX11.so.6')
        x.XOpenDisplay.argtypes=[ctypes.c_char_p]; x.XOpenDisplay.restype=ctypes.c_void_p
        x.XDefaultRootWindow.argtypes=[ctypes.c_void_p]; x.XDefaultRootWindow.restype=ctypes.c_ulong
        x.XCreateSimpleWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_uint,ctypes.c_uint,ctypes.c_uint,ctypes.c_ulong,ctypes.c_ulong]; x.XCreateSimpleWindow.restype=ctypes.c_ulong
        x.XInternAtom.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int]; x.XInternAtom.restype=ctypes.c_ulong
        x.XSetSelectionOwner.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong]
        x.XChangeProperty.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_int,ctypes.c_int,ctypes.c_void_p,ctypes.c_int]
        x.XSync.argtypes=[ctypes.c_void_p,ctypes.c_int]
        x.XCloseDisplay.argtypes=[ctypes.c_void_p]
        display=x.XOpenDisplay(env['DISPLAY'].encode())
        owner=x.XCreateSimpleWindow(display,x.XDefaultRootWindow(display),0,0,1,1,0,0,0)
        selection=x.XInternAtom(display,b'_XSETTINGS_S0',0)
        prop=x.XInternAtom(display,b'_XSETTINGS_SETTINGS',0)
        x.XSetSelectionOwner(display,selection,owner,0)
        for msb in [0,1]:
            order='>' if msb else '<'
            records=[]
            for name,kind,value in [(b'Test/Integer',0,struct.pack(order+'i',42)),(b'Test/Color',2,struct.pack(order+'4H',1,2,3,4)),(b'Net/IconThemeName',1,struct.pack(order+'I',9)+b'LiveTheme'+b'\0'*3)]:
                records.append(struct.pack(order+'BBH',kind,0,len(name))+name+b'\0'*((-len(name))%4)+struct.pack(order+'I',1)+value)
            valid=bytes([msb,0,0,0])+struct.pack(order+'II',1,len(records))+b''.join(records)
            invalid_length=bytes([msb,0,0,0])+struct.pack(order+'II',1,1)+struct.pack(order+'BBH',1,0,17)+b'Net/IconThemeName'+b'\0'*3+struct.pack(order+'II',1,0xffffffff)
            for data,expected_theme in [(valid,'LiveTheme'),(valid[:10],'FileTheme'),(valid[:-4],'FileTheme'),(invalid_length,'FileTheme')]:
                raw=ctypes.create_string_buffer(data)
                x.XChangeProperty(display,owner,prop,prop,8,0,raw,len(data)); x.XSync(display,0)
                actual=subprocess.check_output([str(root/'theme-probe'),'x11'],env=env,text=True,timeout=5).strip()
                assert actual==expected_theme,(msb,expected_theme,actual)
        x.XCloseDisplay(display)
        print('PASS: GTK settings precedence and XSettings in both byte orders, integer/color records, truncated and oversized strings')
    finally:
        server.terminate()
        server.wait(timeout=5)
