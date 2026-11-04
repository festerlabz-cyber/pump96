# BoomTown Beats — Pump 96

Sidechain-pomp zonder sidechain, als **Audio Unit** (Logic Pro) en **VST3**.
Gebouwd met [JUCE](https://juce.com) 9.0.4. De klank is één op één overgezet uit het browserprototype in `prototype/`.

## Downloaden en installeren (Mac)

1. Ga naar het tabblad **Actions** van deze repository en open de bovenste groene run *Build macOS (AU + VST3)*.
2. Download onderaan bij **Artifacts** het bestand `Pump96-…`. Je krijgt een zip; dubbelklik die om de `.dmg` te krijgen.
3. Open de `.dmg`.
4. Sluit Logic Pro, open **Terminal** en plak:

   ```
   bash "/Volumes/Pump 96/install.sh"
   ```

5. Start Logic Pro. De plugin staat bij **Audio FX → Audio Units → BoomTown Beats → Pump 96**.

## Wat de plugin doet

- Een volumecurve die meeloopt met de songpositie van Logic, dus altijd precies op de tel. Staat Logic stil, dan loopt
  de curve vrij door.
- **Curve**: teken je eigen vorm. Sleep punten, klik op een lege plek voor een nieuw punt, dubbelklik (of rechtsklik)
  om er een weg te halen. Punten klikken op een raster; houd Shift ingedrukt om vrij te slepen.
- **Shape**: Classic, Smooth, Hard, Gate, Ramp, Bounce of Custom (je eigen curve, wordt met het project bewaard).
- **Depth** en **Rate** (1/32 tot 2/1). **Feel**: Smooth (rondt harde randen af) en Offset (schuift de curve).
- **Band**: Full, Lows of Highs, met instelbaar Split-punt. **Output** en de actuele Gain.
- Geen latency. 8 presets, waaronder *Gabber Sidechain*, *Bass Duck* en *Hakkuh Stutter*.

## Zelf bouwen

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

## Licenties

- JUCE: dual-licensed (AGPLv3 of de JUCE 9-licentie); gratis Starter-licentie zolang de omzet onder $20.000 per jaar blijft.
- Fonts: Anton (OFL), Inter (OFL), JetBrains Mono (OFL); licenties staan in `Resources/fonts/`.
