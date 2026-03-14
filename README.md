# C-OS (Win7-style Desktop Simulator in C++)

Bu proje, **tam teşekküllü bir işletim sistemi** değil; C++ ile yazılmış, Windows 7 estetiğinden ilham alan bir **masaüstü simülatörü**dür.

## Özellikler

- Win32 API ile çizilmiş masaüstü görünümü
- Altta görev çubuğu ve "Start" butonu
- Basit saat + tarih gösterimi
- Başlat menüsü aç/kapa
- Masaüstü ikonları (Notepad / Computer)
- Start menüsü içinde **15 farklı uygulama** (Productivity, System, Media, Dev, Fun vb.)
- Her uygulamada içerik kartları + metrik/toggle/variant etkileşimleri
- Notepad Pro: not satırı ekleme ve metin içerik akışı
- Game Center / Clock gibi uygulamalarda canlı durum metrikleri
- Fare ile sürüklenebilir pencereler (ekran sınırına clamp)
- Her pencerede kapatma (`X`) butonu

## EXE alma yolları

### 1) GitHub Actions artifact (en kolay)

Repodaki `.github/workflows/build-windows.yml` otomatik olarak Windows'ta derler ve `C-OS.exe` artifact'ini üretir.

- **Actions** sekmesine gir
- Son çalışan `Build C-OS EXE (Windows)` workflow'unu aç
- `C-OS-exe` artifact'ini indir

> Not: `release` işi pull request'lerde **bilerek** skip olur; sadece `v*` tag push'larında çalışır.

### 2) Tag ile direkt GitHub Release'e EXE koyma

Bir tag push edersen workflow `C-OS.exe` dosyasını release asset olarak da yükler.

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

## Windows Defender / SmartScreen notu

`"Bilinmeyen yayıncı"` veya `"bilgisayarınızı tehlikeye atabilir"` uyarıları genelde **imzasız EXE** dosyalarında görülür.
Bu durum yalnızca bu projeye özel değildir.

Bu repo tarafında azaltmak için:

- EXE içine `version info` metadata eklendi (`resources/version.rc`)
- UAC seviyesi `asInvoker` olan manifest eklendi (`resources/app.manifest`) ve MSVC link adımında gömülüyor

Kalıcı çözüm için:

1. Kod imzalama sertifikası (tercihen EV Code Signing) ile `C-OS.exe` imzala.
2. İmzalı dosyayı release üzerinden dağıt.
3. Her sürümde aynı ürün adı/şirket bilgisi kullan.

> Özet: SmartScreen/Defender uyarısını tamamen kaldırmanın güvenilir yolu dijital imzadır.

## Not

Gerçek bir OS (kernel, bootloader, driver stack vb.) geliştirmek için çok daha farklı mimari ve düşük seviye geliştirme gerekir. Bu repo, görsel/etkileşimsel bir "Win7 benzeri deneyim" başlangıcı sağlar.
