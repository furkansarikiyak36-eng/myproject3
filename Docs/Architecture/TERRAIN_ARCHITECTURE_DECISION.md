# Arazi Mimarisi Karar Kaydı (7D-V01 / 7D-V02)

Tarih: 26 Ekim 2026
Durum: **Öneri — ürün sahibinin onayını bekliyor.** Prototip ve ölçümler bu karara göre ilerliyor; onay gelmezse yalnız `UVoxelTerrainChunkComponent` etkilenir.

## Soru

1. **7D-V01:** Kazılabilir arazi tüm dünyada mı smooth voxel olacak, yoksa makro arazi Landscape (heightfield) olup yalnız seçilmiş bölgeler mi smooth voxel olacak?
2. **7D-V02:** Hazır bir voxel eklentisi mi, yoksa motorun kendi parçalarıyla kendi çekirdeğimiz mi?

## Ölçüm (UE 5.8.4, MyProject3Editor Development, bu makine)

Prototip: `Source/MyProject3/VoxelTerrainChunkComponent.*`
Ölçüm testi: `MyProject3.Terrain.Voxel.DigUpdatesSurfaceAndCollision`

- Yoğunluk ızgarası: chunk başına `(N+1)^3` adet float, negatif = katı.
- Yüzey: motorun `UE::Geometry::FMarchingCubes` sınıfı (GeometryCore, paralel).
- Mesh ve collision: `UProceduralMeshComponent`, complex-as-simple, eşzamanlı collision pişirme.

| Chunk | Boyut | Bellek (yoğunluk) | İlk mesh | Kazı + yeniden mesh + collision |
|---|---|---|---|---|
| 32³ hücre, 50 cm | 16 × 16 × 16 m | ~141 KB | 40–54 ms | 38–47 ms |
| 64³ hücre, 50 cm | 32 × 32 × 32 m | ~1,1 MB | — | 95–113 ms |

Beş ayrı çalıştırmanın aralığı. Hepsi oyun iş parçacığında ve eşzamanlı.

Doğrulanan davranış:

- Kazı katı hacmi azaltıyor; doldurma geri ekliyor.
- Çukurun dibine atılan ışın, yeniden kurulumdan hemen sonra yeni yüzeye çarpıyor.

Prototipte bulunan iki hata, kalıcı olarak düzeltildi:

1. Tam ızgara noktasına düşen sıfır yoğunluk iğne deliği bırakıyordu; değerler artık yüzeyin tam üstünde tutulmuyor.
2. Alanı sıfır olan üçgenler eleniyor.

## Değerlendirme

**Tüm dünya smooth voxel (a):**

- 2 × 2 km'lik bir dünyada yalnız 16 m kalınlığında bir yüzey katmanı bile 125 × 125 = 15.625 chunk demek. Ham yoğunluk ~2,2 GB tutar. Seyrek veya sıkıştırılmış depolama, LOD'lu uzak chunk'lar ve akış (streaming) şart olur.
- Ilk yükleme oyun iş parçacığında 15.625 × ~45 ms ≈ 12 dakika sürer; asenkron üretim olmadan imkânsız.
- 7DtD gibi her yer kazılabilir. Baskında kazıcı düşman her yerden girebilir.

**Hibrit (b):** Makro arazi Landscape. Kazı cepleri, mağaralar, tüneller, maden ocakları, set ve hendek bölgeleri voxel chunk.

- 20 cep × 4 chunk = 80 chunk ≈ 11 MB; yükleme yaklaşık 3,6 saniye, zaten asenkrona alınabilir.
- Landscape'in yerleşik araçları (Edit Layers, spline, PCG, foliage, navmesh) makro arazide çalışmaya devam eder.
- Voxel cebinin Landscape ile birleştiği yerde Landscape'te delik (visibility hole) ve dikiş yönetimi gerekir.

**Tüm dünya, uzakta düşük çözünürlüklü voxel (c):** (a) ile aynı bellek ve akış altyapısını ister, üstüne LOD dikişi ekler. İlk sürüm için en pahalı seçenek.

**Eklenti ya da kendi çekirdeğimiz:**

- Makinede kurulu bir voxel eklentisi yok.
- Motorda çalışma zamanında kullanılabilen `FMarchingCubes`, `ProceduralMeshComponent`, `GeometryScripting` ve deneysel `MeshTerrainMode` var.
- Prototip, kazı + collision döngüsünün motorun kendi parçalarıyla ve ~350 satırla kurulabildiğini gösterdi.
- Harici bir eklenti lisans, UE 5.8 uyumu, kayıt formatı denetimi ve kaynak koda erişim riski taşır. Prototip bu ihtiyacı göstermedi.

## Öneri

1. **7D-V01 → (b) hibrit.** Makro arazi Landscape. Kazının oynanışın merkezinde olduğu yerler (kazı cepleri, mağaralar, madenler, hendek ve set bölgeleri, baskın yaklaşma yolları) smooth voxel chunk. Böylece "smooth voxel kazı" talebi oyunun anlamlı yerlerinde tam karşılanır; bellek ve yükleme bütçesi de kontrol altında kalır.
2. **7D-V02 → kendi çekirdeğimiz:** motorun `FMarchingCubes` + `ProceduralMeshComponent` parçaları. Gerekirse sonra `GeometryScripting`/`UDynamicMeshComponent`'e geçilebilir; veri modeli (yoğunluk ızgarası) değişmez.

## Bu karardan doğan zorunlu işler

- **Asenkron mesh üretimi:** 32³ chunk'ta ~45 ms, kare bütçesinin (16,7 ms) üç katı. Kazı yüzeyi bir arka plan görevinde üretilmeli, collision güncellemesi tek commit'te yayınlanmalı. Alternatif: 16³ chunk'lar (tahmini ~6 ms, ayrıca ölçülmeli).
- **Chunk sınırları:** Komşu chunk örnekleri ve dikişsiz yüzey (7D-V04).
- **Kalıcılık:** Seed'den temel + değişen chunk delta'ları (7D-V10). `WorldTerrainStateSubsystem`'in kalıcı makbuz sorunu (TERR-12) de burada çözülmeli.
- **Navmesh:** Değişen sınırlarda yerel yeniden oluşturma (7D-V06).
- **Kazı → item:** Hacim değişimi zaten ölçülüyor; malzeme kimliği ve item üretimi tek işlemde bağlanmalı (7D-V08).
- **Landscape birleşimi:** Cep sınırında Landscape deliği ve geçiş dikişi.
