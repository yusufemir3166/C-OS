# C-OS (Win7-style Desktop Simulator in C++)

Bu proje, **tam teşekküllü bir işletim sistemi** değil; C++ ile yazılmış, Windows 7 estetiğinden ilham alan bir **masaüstü simülatörü**dür.

## Özellikler

- Win32 API ile çizilmiş tam ekran masaüstü görünümü
- Altta görev çubuğu ve "Start" butonu
- Basit saat gösterimi
- Başlat menüsü aç/kapa
- Simüle edilmiş iki uygulama penceresi:
  - Notepad
  - Computer
- Fare ile sürüklenebilir pencereler
- Her pencerede kapatma (`X`) butonu

## Derleme (Windows)

Gereksinimler:

- CMake 3.16+
- Visual Studio 2022 (veya Build Tools)

Komutlar:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Çıktı:

- `build/Release/C-OS.exe`

## GitHub'da EXE üretimi

Repoda `.github/workflows/build-windows.yml` workflow'u vardır.

- Her `push`/`pull_request` için Windows'ta derler
- `C-OS.exe` dosyasını artifact olarak yükler

Böylece GitHub üzerinden indirilebilir bir `.exe` elde edebilirsin.

## Not

Gerçek bir OS (kernel, bootloader, driver stack vb.) geliştirmek için çok daha farklı mimari ve düşük seviye geliştirme gerekir. Bu repo, görsel/etkileşimsel bir "Win7 benzeri deneyim" başlangıcı sağlar.
