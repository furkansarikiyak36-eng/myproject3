# Faz 2 — Voxel Teknolojisi Doğrulama ve Karar Kapısı

**Tarih:** 28 Ekim 2026
**Dal:** `integration/phase-02-14-closure`
**Karar durumu:** İlk oynanabilir dilim için mimari kararı verildi; kaynak uygulaması kısmi, Unreal kabulü açık.

## Karar

- Makro açık dünya yüzeyi UE Landscape / heightfield olacak.
- Smooth voxel yalnızca açıkça tanımlanan kazı cepleri, kısa tüneller, küçük yeraltı odaları, maden cepleri ve hendek/set alanlarında kullanılacak.
- İlk çekirdek UE GeometryCore `FMarchingCubes` + `UProceduralMeshComponent`; üçüncü taraf voxel eklentisi zorunlu kılınmıyor.
- Mevcut 32³ / 50 cm prototip ölçüm tabanı olarak kalıyor. Üretim kare bütçesi için mesh üretimi ve çarpışma güncellemesi asenkronlaştırılmadan geniş kapsamda kullanılmamalı.
- Yumuşak toprak / sert toprak / kaya için prototip alet gücü alt sınırları sırasıyla 1 / 3 / 5. Bunlar denge ayarıdır, nihai oyun değerleri değildir.

## Mevcut teknik kanıt

Önceki dal kayıtlarında UE 5.8.4 için beş çalıştırmada 32³ chunk'ın yoğunluk grid'i yaklaşık 141 KB, ilk yüzey üretimi 40–54 ms ve kazı + mesh + çarpışma 38–47 ms olarak raporlandı. 64³ chunk'ta yoğunluk grid'i yaklaşık 1,1 MB, kazı + yeniden mesh + çarpışma 95–113 ms idi. Bunlar bu dalda yeniden çalıştırılmış sonuçlar değildir; bu turdaki yeni kodun performans kanıtı sayılmaz.

32³ değişikliği yaklaşık tek kare 16,7 ms bütçesini aştığı için kararın anlamı tüm dünyayı eşzamanlı voxel yapmak değildir. Yerel cepler ve sonraki bir asenkron üretim / çarpışma yayınlama adımı zorunludur.

## Bu dalda kaynak seviyesinde yapılanlar

- `UVoxelTerrainChunkComponent` için sınırlandırılmış kazı API'si eklendi: izinli yerel kutu, chunk sınırı, maksimum yarıçap ve beklenen yoğunluk revizyonu kontrolü.
- Zemin sınıfı ve alet gücü kontrolü eklendi. Kaynak API'ye verilen güç değeri oyuncu girdisinden doğrudan kabul edilmemeli; sonraki oyuncu entegrasyonu bunu güvenilir araç/item tanımından çıkarmalı.
- Kazı kimliği makbuzu eklendi. Aynı kimlik + aynı parametreler tekrarlandığında önceki sonuç döner; kimliğin farklı istek için yeniden kullanımı reddedilir.
- Zemine bağlı yapı grafiği köklerinin belirlenmiş 3B güvenlik marjına yakın kazı reddedilir. Bu kaba bir koruma kuralıdır; fiziksel zemin gerilmesi veya destek kaybı simülasyonu değildir.
- Yoğunluk örnekleri, chunk boyutları, revizyon ve kazı makbuzları SaveGame v14 içine kimlik bazında eklenmiştir. Yükleme hedef level'de zaten bulunan chunk'ları doğrular ve mesh/çarpışmayı yeniden kurar.
- `MyProject3.Terrain.Voxel.BoundedDigChecksMaterialToolAndRevision`, `MyProject3.Terrain.Voxel.DigRejectsNearGroundedStructure`, `MyProject3.Terrain.Voxel.DensitySnapshotRoundTrip` ve `MyProject3.Terrain.Voxel.LogicalPhysicalExcavationIsAtomic` otomasyon test kaynakları eklendi; bu turda UE derlemesi veya bu testlerin çalıştırılması yapılmadı.
- `UWorldTerrainStateSubsystem::TryApplyVoxelExcavation` yeni koordinasyon girişidir: önce voxel snapshot alır, fiziksel kazıyı uygular, mantıksal hücre commit'i reddederse fiziksel snapshot'ı geri yükler.

## Kabul tablosu

| Kabul alanı | Kaynak durumu | Motor kabulü |
|---|---|---|
| Yumuşak/sert zemin ve kaya aracı eşiği | Eklendi | Çalıştırılmadı |
| İzinli hacim, yarıçap ve revizyon kontrolü | Eklendi | Çalıştırılmadı |
| Yinelenen işlem kimliği | Eklendi | Çalıştırılmadı |
| Yapı desteği yakınında korumacı red | Eklendi | Çalıştırılmadı; mesafe kuralı sınırlı |
| Yoğunluk + makbuz snapshot doğrulama | Eklendi | Çalıştırılmadı |
| SaveGame v14 / yüklemede mesh-çarpışma üretimi | Kaynak akışı eklendi | Round-trip / rollback denenmedi |
| Landscape ile görünürlük deliği ve geçiş dikişi | Eksik | Bekliyor |
| Komşu chunk dikişsiz yüzeyi ve streaming | Eksik | Bekliyor |
| Asenkron mesh + collision commit | Eksik | Bekliyor |
| Navmesh güncellemesi ve yapı grafiğiyle atomik destek işlemi | Eksik | Bekliyor |
| Kazıdan fiziksel kaynak öğesi çıkarımı | Eksik | Bekliyor |
| Oyuncunun gerçek alet/etkileşim hattı | Eksik | Bekliyor |

## Faz çıkış ölçütü

Faz 2 **tamamlandı** olarak işaretlenmiyor. Mimari tercih belirlenmiş, güvenli kaynak API'si ve kayıt verisi için ilk uygulama yazılmıştır. Faz 2 ancak UE 5.8 UHT/derlemesi, negatif/pozitif otomasyon testleri, gerçek kazı çarpışması, yapı desteği senaryoları ve Save → Dig → Save → Mutate → Load → Trace akışı çalıştırılıp kanıtlandığında motor kabulü alabilir.

## Sonraki bağımlılık sırası

1. Kaynak/derleme sorunlarını UE 5.8'de doğrula.
2. Mantıksal `WorldTerrainStateSubsystem` ve fiziksel voxel yoğunluğunu tek hazırlık/commit/rollback sınırına bağla.
3. Player/tool girdisini güvenilir araç verisiyle `TryApplyBoundedDig` çağrısına bağla.
4. Chunk sınırları, async mesh/collision ve yerel navigation güncellemesini ekle.
5. Yapı kökleri, köprü ayakları ve hendek altındaki destek senaryolarını PIE'de doğrula.
6. Ancak bundan sonra voxel altyapısını daha geniş fazlara yay.