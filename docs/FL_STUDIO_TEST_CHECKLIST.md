# FL Studio manual test checklist
1. Install via the setup EXE; in FL Studio: Options > Manage plugins > Find installed plugins (verify "KITSUNE Auto Tune" appears).
2. Insert on a mixer track; confirm it loads on stereo and mono sources; editor opens, resizes, closes, reopens.
3. Play a sung/recorded vocal: pitch display tracks, CORRECTED line snaps to highlighted scale notes.
4. Retune Speed 0 ms + Strength 100 % = hard-tune; Speed 80 ms + Humanize 70 % = natural. Listen for octave jumps / glitches on consonants.
5. Toggle LOW LATENCY / STUDIO while playing: no crash; latency readout changes; FL's plugin delay compensation re-aligns (check against a dry duplicate).
6. Toggle FORMANT KEEP, move FORMANT; check intelligibility.
7. Turn off individual NOTE buttons: those notes are never targeted.
8. TONE tab: HPF/LOW/MID/HIGH audibly act; enable HARSH REDUCTION on a sibilant vocal; REDUCTION bars move.
9. Automate Speed / Mix from an automation clip; no zipper noise or clicks.
10. Save project, close, reopen: all knobs, key, scale, note toggles, mode restored.
11. Presets: load every factory preset; save a user preset; reload it after restarting FL.
12. A/B and Undo/Redo buttons; Bypass (host bypass too) keeps timing aligned (no jump).
13. Sample rates 44.1 / 48 / 96 kHz; buffer sizes 64–2048.
14. CPU meter stays low (expect a few % of one core).
15. Run `pluginval --strength-level 5` on the .vst3 if available.
