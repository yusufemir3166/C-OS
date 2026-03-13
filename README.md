# C-OS (Win7-style Desktop Simulator in C++)

Bu proje, **tam teşekküllü bir işletim sistemi** değil; C++ ile yazılmış, Windows 7 estetiğinden ilham alan bir **masaüstü simülatörü**dür.

## Özellikler

- Win32 API ile çizilmiş masaüstü görünümü
- Altta görev çubuğu ve "Start" butonu
- Basit saat gösterimi
- Başlat menüsü aç/kapa
- Simüle edilmiş iki uygulama penceresi:
  - Notepad
  - Computer
- Fare ile sürüklenebilir pencereler
- Her pencerede kapatma (`X`) butonu

## EXE alma yolları

### 1) GitHub Actions artifact (en kolay)

Repodaki `.github/workflows/build-windows.yml` otomatik olarak Windows'ta derler ve `C-OS.exe` artifact'ini üretir.

- **Actions** sekmesine gir
- Son çalışan `Build C-OS EXE (Windows)` workflow'unu aç
- `C-OS-exe` artifact'ini indir

### 2) Tag ile direkt GitHub Release'e EXE koyma

Bir tag push edersen workflow `C-OS.exe` dosyasını release asset olarak da yükler.

Örnek:

```bash
git tag v0.1.0
git push origin v0.1.0
```

## Derleme (Windows, native)

Gereksinimler:

- CMake 3.16+
- Visual Studio 2022 (veya Build Tools)

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Çıktı:

- `build/Release/C-OS.exe`

## Derleme (Linux -> Windows cross-compile)

MinGW kuruluysa:

```bash
./scripts/build-exe.sh
```

Çıktı tipik olarak:

- `build-mingw/C-OS.exe`

## Not

Gerçek bir OS (kernel, bootloader, driver stack vb.) geliştirmek için çok daha farklı mimari ve düşük seviye geliştirme gerekir. Bu repo, görsel/etkileşimsel bir "Win7 benzeri deneyim" başlangıcı sağlar.
