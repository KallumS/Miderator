# 0022 - Templates for sections and big ensembles, in the core, in score order

- **Date:** 2026-10-07
- **Status:** Accepted

## Context

New scores could start as nine small ensembles, listed as a flat menu and
spelled out inside the controller. The user asked to start with a full string,
brass or woodwind section, all of the sections, or a jazz big band.

## Decision

- The templates are data in the core (`Templates.*`): a name, the heading it
  is listed under, a line describing it, and instrument ids. The core tests
  check every id exists, so a template cannot name an instrument that is not
  in the table.
- Instruments are listed in **score order**: woodwind, brass, percussion,
  keyboards and harp, voices, strings; a big band's saxes, trumpets,
  trombones, then rhythm section. A repeated instrument becomes numbered
  parts (Horn 1-4); a single one is not numbered.
- The sections follow a standard large orchestra: woodwinds with piccolo, cor
  anglais, bass clarinet and contrabassoon; brass of four horns, three
  trumpets, two trombones, bass trombone and tuba. **Full Orchestra** has the
  woodwinds in pairs with piccolo (28 parts). **Big Band** is the usual 5-4-4
  plus guitar, piano, bass and drums.
- Three instruments were added for them: baritone saxophone (set like the
  tenor, an octave up on an octave clef), vibraphone, and upright bass (plucked,
  so General MIDI's acoustic bass, not the orchestra's bowed double bass).
- The New menu groups them under headings and shows how many parts each has.
- Part names that do not fit the margin are shortened the way a printed score
  shortens them, keeping the number: Tenor Saxophone 2 becomes T. Sax. 2.

## Consequences

- Adding a template is one line, with no app code.
- Big templates need more than one synth to sound right (0023).
