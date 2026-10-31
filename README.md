# Scriba Monitor — Meteora Plugins

Correzione acustica della stanza per monitor da studio. Formati: VST3, AU e Standalone.
Nella DAW il plugin appare come **Scriba Monitor (Meteora Plugins)**.

- **Standalone**: misura della stanza (check del livello, sweep su più punti, salvataggio del profilo).
- **Plugin**: correzione in tempo reale con il profilo misurato, target, regolazioni ed emulazioni di monitor.

## Compilare

Serve CMake 3.22 o successivo. JUCE 8.0.4 viene scaricato automaticamente; se c'è una cartella `JUCE/`
accanto al progetto, viene usata quella.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Con GitHub Actions (`.github/workflows/build.yml`) a ogni push si ottengono le build per macOS
(universal: VST3, AU, app) e Windows (VST3, exe), più il test del motore DSP.

Test del motore DSP (stanza simulata), da eseguire nella cartella del progetto:

```
cmake -S . -B build-tests -DSCRIBA_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --target ScribaDspTest
./build-tests/ScribaDspTest_artefacts/Release/ScribaDspTest
```

## Come funziona

1. **Check del livello**: 2 s di silenzio (rumore di fondo) + sweep breve. Zona ottimale del picco
   da −12 a −6 dBFS, massimo −3 dBFS, SNR minimo 40 dB.
2. **Misura**: per ogni punto, sweep esponenziale di 6 s sul monitor sinistro e poi sul destro.
   La risposta all'impulso si ottiene per deconvoluzione; la latenza viene rilevata dal picco.
3. **Calibrazione**: la curva del microfono viene sottratta (default: Behringer ECM8000 generica).
4. **Profilo**: media in potenza dei punti, salvata in `Documenti/Meteora Plugins/Scriba Monitor/Profiles`
   (`.scribamon`, JSON). Il plugin ricalcola il filtro dal profilo alla frequenza di campionamento della sessione.
5. **Filtro**: target − misura, con limiti di boost/taglio, niente boost sotto il roll-off dei monitor,
   bilanciamento di livello L/R; FIR a fase minima (latenza zero) o fase lineare (latenza = metà FIR).

## Struttura

| Cartella | Contenuto |
|---|---|
| `Source/dsp` | Motore: griglia di frequenza, sweep e deconvoluzione, calibrazione, progetto del filtro, emulazioni, profili |
| `Source/MeasurementSession.*` | Sessione di misura nello standalone (thread audio + analisi) |
| `Source/PluginProcessor.*` | Parametri, convoluzione, stato |
| `Source/gui` | Look and feel, grafico, pagine Misura e Correzione |
| `Resources` | Curva di calibrazione ECM8000 |
| `tests` | Test del motore DSP |

## Da fare prima del rilascio

- **Curva ECM8000**: `Resources/ECM8000_generic.cal` contiene la misura Cross-Spectrum Labs di un esemplare
  (1/3 di ottava, 0 gradi). Il microfono va quindi puntato verso i monitor, non verso il soffitto.
  Chi ha un file del proprio esemplare può caricarlo nella pagina Misura.
- **Emulazioni**: le curve in `FilterDesigner.cpp` (`getEmulations`) sono punti di partenza;
  rifinirle con misure reali dei sistemi di riferimento.
- **Grafica**: stile della famiglia Scriba (Scriba Align), palette in `Source/gui/ScribaLookAndFeel.h`.
  Il logo nell'intestazione è un segnaposto (`ScribaMonitorEditor::drawLogo`): sostituirlo con il logo originale.
- **Scelta della scheda audio dentro la pagina Misura**: per ora si usa il menu Opzioni dello standalone di JUCE.
- **Allineamento temporale L/R** (ritardi) e supporto subwoofer.
