#!/bin/bash

xcp=$HOME/.config/xlunch/
xsp=$HOME/.local/share/xlunch
ni=/usr/share/icons/hicolor/48x48/apps/xlunch.png
mkdir -p $xcp
mkdir -p $xsp/svgicons
sh /usr/bin/genentries >$xcp/entries.dsv
(notify-send -a xlunch -i $ni "Done generating new entries" && \
    mv -f svgicons/* $xsp/svgicons && \
    rm -r svgicons/ && \
    (cat $xcp/entries.dsv | sed 's|;svgicons\/|;.local/share/xlunch/svgicons/|' >entries2.dsv && \
        mv entries2.dsv $xcp/entries.dsv)) \
    || notify-send -a xlunch -i $ni "Error generating new entries"
