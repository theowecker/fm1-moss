# MOSS-1 manual

MOSS-1 is ambient firmware for the M-VAVE FM-1. Turn it on and it starts playing. Play the keys and the music
picks up what you play.

![](img/drift-dusk.png)

## The short version

- **PRESETS** picks the scene. There are 16, across three modes.
- **ALGORITHM** picks the space: the character of the reverb and delay.
- **SELECT** steps through the pages in order: the sound (HOME, MIX, FX, MOTION), the beat, the keys, CHORDS and
  SETUP.
  The loop pages have their own screen: press LFO. **K1 to K4** change the page you are on.
- **The keys** play along, in key.
- **Play** and what you play loops by itself. **REC** keeps a loop or clears it. **ARP** freezes the sound.
- **SEQ** steps through the three screens: the picture, the studio and the loops.
- **SAVE** saves your settings and your loops.

## The panel

| Control | What it does |
| --- | --- |
| PRESETS | Next or previous scene |
| ALGORITHM | Next or previous space |
| SELECT | Next or previous page |
| HOME | The HOME page |
| SEL | The MIX page. Press again for BEAT |
| ENV | The KEYS page. Press again to change how the keys play (PLAY, LISTEN or HOLD) |
| LFO | The LOOPS screen, on its LAYERS page. Press again for SHAPE, TAPE, then LIBRARY |
| FX | The FX page. Press again for MOTION |
| GLO | The SETUP page. Press again for CHORDS |
| K1 to K4 | The four knobs of the page you are on |
| REC | The looper: tap to keep the loop from fading (tap again to let it fade). Hold for a second to clear it. See [Looping](#looping) |
| ARP | Freeze: the reverb holds what it has and new sound plays dry over it. In SCAPES and DRIFT the chord also stops changing. Press again to let go |
| PLAY | Start or stop the music. The screen says RESTING while it is stopped. Your loop keeps going |
| EDIT | A new variation: new chords, new bell patterns in DRIFT, new weather in SCAPES. Your loop is not touched |
| SEQ | The next screen: PICTURE, STUDIO, LOOPS |
| SAVE | Save your settings and your loop library. Hold for two seconds and let go to reset the scene. Hold for six to reset everything (your saved loops stay) |
| OCT- and OCT+ | Move the keys down or up an octave, from -2 to +2 |
| MASTER | Volume |

The lit page button shows which page you are on. SAVE stays lit while there are changes you have not saved. Lit
keys show the notes of the chord that is playing.

## Modes and scenes

**LOFI**: electric piano chords on a slow, swung beat with a soft bass. Study, Rainy, Night, Tape.

**SCAPES**: nature with a quiet pad and a few notes. Forest, Rain, Shore, Night, Storm, Hearth, Creek.

**DRIFT**: slow pads and bells on loops of different lengths, so it never quite repeats. Halo, Tide, Glass, Dusk,
Garden. Garden is the bright one: quicker kalimba loops, birdsong and a soft beat.

PRESETS goes through them all in one list: LOFI, then SCAPES, then DRIFT, then back to the start. MOSS-1 starts
on DRIFT Garden.

Each scene sets its own HOME and MIX knobs, tempo, mood and beat. Changing scene loads them. The KEY, the space
and the FX, MOTION, KEYS, SHAPE and TAPE pages stay as you set them, whatever the scene.

To get a scene back to how it started, hold SAVE until the screen says LET GO TO RESET SCENE, then let go. That
resets its HOME and MIX knobs, tempo, mood and beat.

To put everything back to how MOSS-1 first starts, keep holding SAVE until the screen says LET GO TO RESET
EVERYTHING (about six seconds), then let go. Every setting goes back, the loop playing is cleared and the loop
library is kept. Press SAVE afterwards to keep it that way.

![](img/lofi-study.png)

![](img/scapes-creek.png)

![](img/drift-halo.png)

## The pages

Turn SELECT, or press a page button. In the studio view the page name is at the bottom left. In the picture view,
turning a knob shows its name and value for a moment.

### HOME

The main knobs. They change with the mode.

| | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| LOFI | TONE: dark to bright | DUST: crackle, then fewer bits | WOBBLE: tape wow | REVERB: size and length |
| SCAPES | WEATHER: calm to wild | LIFE: how many birds and crickets | TONE | REVERB |
| DRIFT | DENSITY: how many bell notes | TONE | DRIFT: how fast things change | REVERB |

### MIX (SEL)

The level of each part.

| | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| LOFI | KEYS: the piano | BEAT | BASS | RAIN |
| SCAPES | NATURE: wind, rain, water, fire | LIFE: birds and crickets | PAD | NOTES |
| DRIFT | PAD | BELLS | BASS | AIR |

### FX (FX)

![](img/fx-page.png)

| Knob | What it does |
| --- | --- |
| DELAY | How much goes into the delay. Low it sits behind the music; past halfway the repeats start to take over |
| TIME | 1/16 to 1/2, in time with the tempo |
| REPEATS | How long the echoes last. All the way up they build and smear but don't run away |
| FILM | Old tape: wobble, hiss, crackle, saturation, a duller top and the odd dropout |

### MOTION (FX again)

What the delay and reverb do over time. Turn DELAY up on the FX page to hear it best.

| Knob | What it does |
| --- | --- |
| MOD | Bends the echoes' pitch slowly, from a slight chorus to seasick |
| RANDOM | Now and then, by chance: the delay jumps to another time for a few seconds (its pitch bends on the way), the echoes swell almost endless for a moment, or the reverb blooms. Higher means more often |
| TONE | The echoes' colour. Left of noon darker with each repeat, right of noon thinner and brighter |
| SPREAD | Down, the echoes stay in the middle. Up, they bounce left and right |

### BEAT (SEL twice)

Drums and a backing bass, in any mode. The LOFI scenes and DRIFT's Garden start with a kit. The others start
with none.

| Knob | What it does |
| --- | --- |
| KIT | OFF, SOFT, LOFI, BRUSH, PULSE, TICK, WOOD |
| PATTERN | AUTO, BOOMBAP, HALF, FOUR, BOSSA, HEART, SHUFFLE, BROKEN, WALTZ, SPARSE. AUTO is the scene's own groove in LOFI and SPARSE elsewhere |
| LEVEL | How loud the drums are, and the ROOT or WALK bass |
| BASS | AUTO (the mode's own), OFF, ROOT (on the chord root, with the kick), WALK (moves through the chord) |

### KEYS (ENV)

![](img/keys-page.png)

| Knob | What it does |
| --- | --- |
| SNAP | FREE plays the note you press. SCALE moves it to the nearest note in the key. CHORD moves it to the nearest note of the chord playing now |
| HARMONY | Adds softer chord notes under each note you play |
| VOICE | The sound of the keys. AUTO uses the mode's own sound |
| TAP | How the keys play: PLAY, LISTEN or HOLD. Pressing ENV again on this page does the same |

In short: **PLAY** plays. **LISTEN** plays, loops and steers the music. **HOLD** plants notes that play themselves.

**PLAY**: the keys just play.

**LISTEN** (where it starts): the music listens to you.

- What you play starts a loop by itself. Stop for two seconds and it closes and repeats, fading and changing a
  little each time.
- Notes you play over a running loop are added to it. Each phrase after a pause goes on its own track.
- In SCAPES and DRIFT, a note outside the chord moves the music to a chord that has it.
- DRIFT puts your note into one of its bell loops. LOFI's melody plays your notes back. In SCAPES a bird answers.
- The more you play, the busier the music gets for a while: more bells, more birds, more ghost notes and fills.

To play over a loop without adding to it, switch TAP to PLAY.

**HOLD**: grow the music by planting notes. It starts with nothing planted, so you hear the scene on its own.

- Tap a key to plant its note. It chimes once, then plays by itself every few seconds, a little differently each
  time: a pluck in some octave, a two-note figure, a low swell or a high sparkle, in a bell, kalimba or piano
  sound, somewhere left or right.
- Tap a lit key to pull that note out.
- Up to eight notes. A ninth replaces the oldest.
- The lit keys are the notes planted. Leaving HOLD pulls them all.
- SNAP still applies, so with SCALE or CHORD a planted note fits the key.

In HOLD the keys don't loop, so REC has nothing to record.

### The LOOPS screen

LFO opens it, or press SEQ until it shows. It has the four loop pages and nothing else: turn SELECT to go through
LAYERS, SHAPE, TAPE and LIBRARY, and the tabs at the top show where you are. Any other page button takes you back
to the screen you came from.

Under the tabs, the looper says what it is doing and what to do next:

- **LISTENING** (the dot pulses): TAP is on LISTEN and it is waiting for you. Play and a loop starts by itself.
- **RECORDING LAYER 1** (red): it is taking down what you play. Stop playing and it loops.
- **ADDING LAYER 2** (red): you are playing over the loop. Pause and the phrase joins as its own layer.
- **FADING** or **KEPT**: the loop is playing. The line under it takes turns with tips: tap REC to keep it, tap
  again to let it fade, hold REC for a new loop.

The lanes are the layers, the dots are their notes and the line is the playhead. On the right, each layer's level,
OFF, or - when it is empty. TAPE shows on the lanes too: WARBLE bends them and AGE browns the notes.

**One loop, then another**: play a phrase, let it loop, keep it if you like. When you want something new, hold REC.
The old loop stays in the library and the next thing you play starts a new one.

### LAYERS (LFO)

![](img/loop-page.png)

A loop holds up to eight layers. This page works on one at a time.

| Knob | What it does |
| --- | --- |
| LAYER | Picks the layer, 1 to 8. Its lane is marked and its number shows on the right |
| LEVEL | That layer's level. All the way down is OFF, which mutes it |
| REDO | Turn right to redo that layer: it plays on until you start playing, then what you play replaces it. Its lane pulses red while it waits. Turn left to change your mind |
| CLEAR | Turn right and the screen says PRESS REC TO CLEAR. Press REC within three seconds and that layer is erased. Turning left or waiting lets it go, so a knob turned by accident never erases anything |

A new layer always starts at the same level, even where one was turned off before. When all eight are used, new
notes go on layer 8.

### SHAPE (LFO again)

| Knob | What it does |
| --- | --- |
| FADE | How fast the loop dies away while it is not kept. When every note has faded, the loop is gone |
| MUTATE | How often notes move to a nearby note, jump an octave or move in time |
| SHIFT | Slide the whole loop a sixteenth later (right) or earlier (left) for each click |
| SPEED | 1/2, 1x, 2x, REV, REV 1/2 |

### TAPE (LFO a third time)

The loop's own tape. These change only the loop, not the rest of the music.

| Knob | What it does |
| --- | --- |
| WARBLE | Wobbly pitch, like a stretched tape |
| DIRT | Saturation, then grit as you turn it up |
| AGE | A copy of a copy: darker, thinner, narrower and a little quieter |
| SMEAR | Backwards swells under your notes, and softer, slower repeats |

### LIBRARY (LFO a fourth time)

Your last eight loops, kept for you. The LOOPS screen lists them, newest first, with > by the one playing.

| Knob | What it does |
| --- | --- |
| LOOP | Turn to a loop and it starts playing |
| DELETE | Turn right, then press REC within three seconds: that loop is taken out of the library |

### CHORDS (GLO twice)

The harmony under everything, always in the KEY and MOOD on SETUP. The studio screen's wheel shows the chord
playing.

| Knob | What it does |
| --- | --- |
| CHORD | AUTO lets the scene choose. Turn it to I to VII and the music moves to that chord of the key, then stays there. In C major, IV is F and VI is A minor. Playing doesn't move a chord you picked |
| CHANGE | How often the chords move by themselves: STILL (they don't), SLOW, NORMAL or OFTEN |
| PATH | The order the chords go in. SCENE is the scene's own. CIRCLE is vi ii V I, DREAM I vi IV V, MOODY vi IV I V, SONG I V vi IV, DRONE just I and IV |
| COLOR | LUSH: 7ths and 9ths, as the scenes are made. PLAIN: simple three-note chords. SUS: a 4th in place of the 3rd, open and unresolved. OPEN: a 2nd in place of the 3rd, airy |

LOFI changes chord on the next bar, so it stays in time with the beat. SCAPES and DRIFT drift into it within a second.

**Two chords, slowly**: CHANGE to SLOW, PATH to DRONE.

**Stay on one**: CHORD to VI for a minor, wistful hold. Turn it to IV later for a lift.

### SETUP (GLO)

| Knob | What it does |
| --- | --- |
| TEMPO | Beats per minute |
| KEY | The key everything plays in |
| MOOD | MAJOR, MINOR, DORIAN, LYDIAN or MIXOLYD (Mixolydian) |
| SLEEP | Fades out and stops after 10 to 90 minutes |

SLEEP fades the sound over the last third of the time, five minutes at most. At the end the screen says
GOODNIGHT, everything goes quiet (the keys too) and SLEEP turns itself off. Press PLAY to start again. Changing
SLEEP starts the timer again. The studio view shows the minutes left.

The CPU number on this page shows how hard the FM-1 is working.

## Spaces

Turn ALGORITHM. Each space changes the whole sound, not just the reverb, and works with any scene.

| Space | Sound |
| --- | --- |
| HALL | A big, soft room |
| PLATE | Shorter and brighter |
| CLOUD | Huge, slow and blurred |
| SHIMMER | A glowing octave above the reverb |
| TAPE | The whole mix on a wobbly tape, its echoes dark and running into the reverb |
| REVERSE | Everything swells in backwards |
| DEEP | An octave below everything, in a dark cave |
| DUB | Dotted echoes that keep going, dark and thin, over a small springy room |
| DREAM | A lush, wide chorus in a soft, shimmering room |
| RADIO | Thin, narrow and a little driven, like a small transistor radio |
| WATER | Muffled, slow and swaying, as if under the surface |
| GLITCH | The echoes stutter and jump between short times |
| PULSE | The sound throbs and pans from side to side in eighth notes |

DUB and GLITCH lean on the delay, so they sound like themselves even with DELAY low. The MOTION and FX knobs still
work on top.

## Looping

![](img/loop.png)

With TAP on LISTEN (where it starts), you just play:

1. Play a few notes and stop. After two seconds of quiet they loop.
2. Play again and the new phrase goes on top as a new layer, up to eight.
3. The loop slowly fades while you leave it, and changes a little each time round (SHAPE sets how much).
4. **Tap REC to keep it.** It stops fading. Tap REC again to let it fade.
5. **Hold REC for a second to clear it** and start a new one.

With TAP on PLAY nothing loops by itself. Tap REC to start recording, play, and tap REC again to loop it.

**Every loop goes into the library by itself.** The one playing is kept up to date as you add layers, and when you
clear it and start a new one, the old one stays in the list. The library holds your last eight. To keep them after
you turn the FM-1 off, press SAVE (SAVE stays lit while there is something to save). Writing to the flash stops the
sound for a moment, so it only happens when you press SAVE.

When there is a beat (LOFI, or a kit on the BEAT page), the loop is a whole number of bars, 1, 2, 4 or 8, and its
notes sit on the beat's grid with its swing, so it stays in time with the drums. Without a beat the loop keeps the
timing you played.

The bar at the bottom of the picture shows the loop: red while recording, then FADING or KEPT. The eight squares
next to it are the layers: bright when on, grey when off, dark when empty. REC's light blinks fast while recording,
slowly while the loop fades, and stays on when it is kept.

A recording stops by itself after 60 seconds. A loop holds 96 notes; past that, new notes replace the quietest.
The loop keeps playing when you press PLAY to stop the music, so you can play over silence. HOLD's notes are not
looped.

## The three screens

Press SEQ.

**Picture**: a pixel landscape for the scene. Notes light up fireflies, windows or flowers. Rain, wind, birds and
lightning follow the sound. FROZEN at the top means ARP's freeze is on.

**Studio**: the notes. The wheel has all twelve notes in fifths. The chord is lit and joined by lines, and each note
you hear blooms where it falls. The bars on the left are the part levels. The column on the right shows DRIFT's
loops, the beat when a kit is on, or SCAPES's weather. The knobs are at the bottom.

![](img/studio.png)

**Loops**: the looper on its own, with only the loop pages. See [The LOOPS screen](#the-loops-screen).

## MIDI

Over USB, the keys send notes on channel 1, and notes from the computer play like the keys.

## Recipes

**Falling asleep**: SCAPES Rain or Night. SETUP, SLEEP to 30m. Turn MASTER down.

**Study**: LOFI Study. FX, FILM to 40. MIX, BEAT lower if it distracts.

**Play a little, listen a lot**: DRIFT Dusk, space CLOUD. Play three or four notes and stop. SHAPE, FADE low and
MUTATE high.

**Backwards garden**: DRIFT Halo, space REVERSE. SHAPE, SMEAR to 70. Play slowly.

**A garden you plant**: any scene, TAP on HOLD (press ENV twice). Plant three or four keys and leave it. Pull one
and plant another now and then.

**Echoes that wander**: FX, DELAY to 60 and REPEATS to 70. MOTION, MOD to 50 and RANDOM to 60. Play a few slow
notes and let the delay move them around.

## Recording

The FM-1 shows up on your computer as an audio input over USB. Record it in any audio app, or use the tape deck
(https://theowecker.github.io/fm1-moss/tape/) in Chrome or Edge:

1. Plug in the FM-1 and press Connect. **Play here** sends the FM-1's sound through your computer's speakers or
   headphones too, a few milliseconds late. It remembers the setting.
2. Press the big button, or space, to record. Press it again to stop.
3. Each take can be named, played back and deleted. Takes stay in the browser until you delete them.
4. **WAV** downloads the take at full quality, for a video editor.
5. **Make video** turns the take into an MP4 with a scene picture (or your own) as the image, ready to upload to
   YouTube. Pick the picture above the list.

The same page reads your loop library (Read loops) and downloads each loop as a MIDI file, one MIDI track per
layer.

## Going back to stock firmware

Open <https://thefirmwarehub.com> in Chrome or Edge with the FM-1 plugged in and install the M-VAVE firmware.

If the FM-1 ever stops starting, hold OCT- while you turn it on. It comes up in the chip's own update mode, and
<https://thefirmwarehub.com/tools/rescue/> puts the stock firmware back.
