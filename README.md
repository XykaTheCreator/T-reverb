# Super Reverb  (VST3)

Özellikler
- 4 mod: Room, Hall, Plate, Cathedral (8 hatlı FDN reverb)
- Mix, Decay (0.2-30 s), Size, Pre-Delay, Damping, Diffusion, Modulation, Width
- Shimmer (oktav yukarı geri besleme)
- Freeze (sonsuz tutma)
- Ducking (kuru sinyal yükselince reverb kısılır) + Release
- Wet için Low Cut / High Cut
- Özel koyu arayüz

## Derleme (bilgisayarına bir şey kurmadan)
1. github.com'da ücretsiz hesap aç, yeni bir BOŞ repo oluştur.
2. Bu klasörün içindekileri (CMakeLists.txt, Source, .github, README.md) repoya yükle.
   (Add file > Upload files. ".github" klasörünün de yüklendiğinden emin ol.)
3. Repoda "Actions" sekmesine git. "Build VST3" otomatik başlar (~10 dk).
4. Bitince çalışmanın en altındaki "Artifacts" bölümünden indir:
   - SuperReverb-VST3-windows-latest  (Windows)
   - SuperReverb-VST3-macos-latest    (macOS)
5. Windows: içindeki "Super Reverb.vst3" klasörünü şuraya kopyala:
   C:\Program Files\Common Files\VST3
6. FL Studio > Options > Manage plugins > "Find installed plugins".
   Eklenti "Super Reverb" adıyla Effects altında görünür (Fruity Wrapper ile).
