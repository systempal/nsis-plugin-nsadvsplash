# NewAdvSplash NSIS Plugin

**Versione personale modificata**

---

## Descrizione Originale

NewAdvSplash.dll - plug-in che permette di visualizzare uno splash screen negli installer NSIS con effetti di dissolvenza (win2k/xp) e trasparenza. Può contemporaneamente riprodurre file audio wav/mp3.

## Utilizzo

### 1) Mostra splash screen

```nsis
newadvsplash::show [/NOUNLOAD] Delay FadeIn FadeOut KeyColor [/BANNER] [/PASSIVE] [/NOCANCEL] FileName
```

**Parametri:**
- `Delay` - tempo (millisecondi) di visualizzazione dell'immagine
- `FadeIn` - tempo per la scena di fade in
- `FadeOut` - tempo per la scena di fade out
- `KeyColor` - colore usato per la trasparenza, può essere qualsiasi valore RGB (es. R=255 G=100 B=16 -> KeyColor=0xFF6410). Usare KeyColor=-1 se non c'è colore trasparente. Se KeyColor=-2 e il tipo è gif, il plug-in tenta di estrarre il colore di trasparenza dall'header del file.
- `/BANNER` - restituisce il controllo all'installer subito dopo l'attivazione del plug-in.
- `/NOCANCEL` - disabilita il comportamento default 'exit on user click'.
- `/PASSIVE` - non forza la finestra splash in primo piano.
- `FileName` - nome file immagine splash (con estensione!). Tipi supportati: Bmp, gif e jpg.

**Esempi:**

```nsis
newadvsplash::show 3000 0 0 -2 "$PLUGINSDIR\catch.gif"
```

```nsis
newadvsplash::show /NOUNLOAD 1000 600 400 0xFF6410 /BANNER "$TEMP\spltmp.bmp"
```

### 2) Ferma splash

```nsis
newadvsplash::stop [/WAIT | /FADEOUT]
```

- Senza opzioni termina il banner.
- Con `/WAIT` attende la fine.
- Con `/FADEOUT` forza la chiusura con effetto fade out.

### 3) Riproduci audio

```nsis
newadvsplash::play /NOUNLOAD [/LOOP] FileName
```

- `FileName` - file audio da riprodurre (con estensione, wav, mp3...).
- Stringa vuota "" ferma l'audio.

### 4) Ottieni handle finestra

```nsis
newadvsplash::hwnd /NOUNLOAD
Pop $0 ; $0 è ora l'handle della finestra
```

## Compatibilità

- Base - Win95 e successivi.
- Fadein/fadeout - win2k/winxp.

## Crediti

- Codice originale - Justin
- Convertito in plugin DLL da Amir Szekely (kichik)
- Dissolvenza e trasparenza di Nik Medved (brainsucker)
- Supporto gif e jpeg, /BANNER e /CANCEL, supporto mp3 - Takhir Bedertdinov

---

## ⚠️ Differenze nella versione personale

### Nuova architettura supportata: x64 (amd64-unicode)

L'originale supportava solo:
- x86-ansi (Plugins\newadvsplash.dll)
- x86-unicode (Unicode\plugins\newadvsplash.dll)

La versione personale aggiunge il supporto per:
- **amd64-unicode** (x64)

### File aggiunti

- `build_plugin.py` - Script Python per compilare il plugin per tutte le architetture
- `src/exdll.h` - Header per l'API NSIS extern DLL
- `src/NewAdvSplash.vcxproj` - Progetto Visual Studio aggiornato per VS2022 e x64

### File rimossi

I file DLL precompilati sono stati rimossi dalla distribuzione in quanto vengono generati dalla compilazione:

- `Plugins/newadvsplash.dll`
- `Unicode/plugins/newadvsplash.dll`

### Compilazione

```cmd
cd nsAdvsplash
python build_plugin.py
```

I DLL vengono copiati in `dist/{platform}/newadvsplash.dll`.

### Opzioni build

```powershell
python build_plugin.py --config x86-unicode      # Solo un'architettura (x86-ansi|x86-unicode|x64-unicode|all)
python build_plugin.py --vs-version 2026         # Versione VS specifica (2022|2026|auto)
python build_plugin.py --clean                   # Pulizia dist/ prima della build
python build_plugin.py --install-dir "C:\NSIS\Plugins"  # Copia in directory NSIS aggiuntiva
python build_plugin.py --verbosity minimal       # Verbosità MSBuild (quiet|minimal|normal|detailed|diagnostic)
```

### Modifiche funzionali

Nessuna modifica funzionale rispetto all'originale. Le modifiche riguardano solo l'infrastruttura di build e il supporto x64.

---

*See [README.md](README.md) for the English version.*
