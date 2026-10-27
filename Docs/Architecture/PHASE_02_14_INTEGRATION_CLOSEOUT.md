# Faz 02–14 Uygulama Kapatma Özeti

**Tarih:** 28 Ekim 2026
**Dal:** `integration/phase-02-14-closure`
**Taban:** `feature/voxel-terrain-policy-spike`
**Genel durum:** Kaynak kod katmanları bir araya getiriliyor; Unreal Engine derleme/PIE, canlı oynanış ve son demo bu çalışmada yapılmadı. Hiçbir faz yalnızca dosya bulunduğu için motor kabulü almış sayılmaz.

## Bu turda gerçekten değiştirilen kaynak

- `VoxelTerrainChunkComponent.h/.cpp`: bounded dig, zemin sınıfı/a let gücü eşikleri, izinli alan/yarıçap, revizyon koruması, tekrar işlem makbuzu, yapı kökü yakınında korumacı red ve yoğunluk snapshot API'si.
- `WorldSimulationSaveGame.h`: şema 14 ve `FWorldVoxelChunkSnapshot` verisi.
- `WorldSaveCoordinatorSubsystem.cpp`: voxel chunk'larının kararlı actor kimliğiyle kaydedilmesi, kayıt seti doğrulaması, yüklemede yoğunluk geri yüklenmesi ve mesh/çarpışmanın yeniden kurulması; v13'ten v14'e fail-closed geçiş.
- `MyProject3CoreTests.cpp`: sınırlı kazı/alet gücü/revizyon, yapı kökü koruması ve yoğunluk snapshot round-trip için üç otomasyon testi kaynağı.
- Faz 2 ve Faz 8/14 belgeleri güncellendi; mimari karar hibrit Landscape + yerel smooth voxel cepleri olarak kayda alındı.

## Fazlara göre gerçek durum

| Faz | Kaynak tabanı | Kalan ana iş / kabul şartı |
|---|---|---|
| 02 — Voxel teknolojisi ve ortak sınırlar | Ölçülmüş tek chunk prototipi; bu dalda sınırlı kazı ve snapshot kaynağı eklendi | UE build/UHT, gerçek testler, mantıksal-fiziksel atomik işlem, oyuncu/a let entegrasyonu, chunk dikişleri, async mesh ve navmesh |
| 03 — Kütük ve ahşap işleme | `LogBuildPiece`, `LogCarryComponent`, işleme/mesh varyantı ve snap çekirdeği mevcut | Tüm kesim/yarma/oluk/kalas negatif durumları, transaction rollback, mesh envanteri ve PIE kabulü |
| 04 — Taş ve duvarcılık | `StoneBuildingPiece` ve materyaller arası bağlantı katmanı mevcut | Tam kullanıcı döngüsü, ölçek/pivot/çarpışma, taş destekleri ve PIE kabulü |
| 05 — Çubuklar ve ilkel araçlar | `StickPrimitivePiece` ve ilişkili tip/anchor katmanları mevcut | Bütün tuzak/çubuk/alet girdilerinin ortak sahiplik ve kaynak tüketimine bağlanması; negatif senaryolar |
| 06 — Envanter ve üretim tarifleri | `InventoryCraftingComponent`, item ownership, storage ve production kaynakları mevcut | Dünya pickup/drop/UI/iptal akışı, fiziksel malzeme tüketimi, kapasite sınırları ve kayıt/yükleme testleri |
| 07 — Yapı grafiği, destek ve hasar | `WorldStructuralGraphSubsystem`, ikincil destekler, hasar ve snapshot katmanı mevcut | Her malzeme adaptörünün tam kaydı, destek kaybında reddetme/çökme politikası, terrain ile atomik destek güncellemesi |
| 08 — Arazi ve sınırlı kazı | `WorldTerrainStateSubsystem` + voxel chunk; bu dalda yoğunluk v14 kaydı ve destek marjı ekli | Landscape Builder gerçek üretimi, görünürlük deliği/kenar dikişi, komşu chunk, async collision/navmesh, item yield ve oyuncu etkileşimi |
| 09 — Tarım ve hayvanlar | `WorldAgricultureSubsystem` ve `WildlifeActor` mantık katmanları mevcut | Gerçek plot/habitat/etkileşim aktörleri, fiziksel item akışları, mevsim ve kayıt entegrasyonu |
| 10 — Baskın ve tehdit | `WorldThreatDirectorSubsystem` ve `WorldRaidAdapterSubsystem` kaynakları mevcut | Hedef seçimi, yol bulma, fiziksel saldırı, arazi/yapı ile gerçek etkileşim ve adil kayıp sınırları |
| 11 — Lojistik ve taşıma | Cargo/route/storage kaynak katmanları mevcut | Sürükleme/kızak/zipline gibi fiziksel taşımanın aynı kimlik ve collision ile uçtan uca kabulü |
| 12 — Basit üretim | `WorldProductionSubsystem` ve istasyon/recipe/queue kayıtları mevcut | Gerçek istasyon aktörleri, yakıt ve physical output, iptal/rollback ve envantere atomik teslim |
| 13 — Hayatta kalma ve keşif | Survival, weather, clock, discovery ve physical health kaynakları mevcut | Ekipman/yiyecek/ateş/uyku kararlarının oyuncu etkileşimi, çevresel tehlikeler ve kayıt/oyun içi kabul |
| 14 — Entegrasyon, kalıcılık ve demo | `WorldSaveCoordinatorSubsystem`, actor/component snapshots ve bu dalda voxel yoğunluk snapshot v14 | Eksik aktör/voxel chunk yeniden yaratma, tam domain transaction rollback doğrulaması ve M001–M119 demo kanıtı |

## Entegrasyon uyarıları

1. `main` bu çalışma dalıyla aynı değildir; bu dal `main` üzerine yazılmadı ve birleştirme yapılmadı.
2. `WorldTerrainStateSubsystem` mantıksal hücre editleri ile `UVoxelTerrainChunkComponent` fiziksel yoğunluğu henüz tek atomik commit değildir. Ayrı ayrı başarılı olmaları birlikte doğru olduklarını kanıtlamaz.
3. Voxel kayıt yüklemesi, etkin chunk aktörlerinin mevcut level'de ve doğru benzersiz kimliklerle bulunmasını bekler; eksik chunk spawn etmez.
4. Yapı desteği koruması bir mesafe marjıdır; toprak gerilmesi veya destek yükünün fiziksel simülasyonu değildir.
5. `feature/voxel-terrain-policy-spike` dalındaki önceki ölçümler önceki koşuya aittir; bu daldaki son değişikliklerden sonra yeniden ölçülmemiştir.
6. Kaynak testi eklemek testin geçtiği anlamına gelmez. Bu dalda UE 5.8 UHT/derleme, Automation, PIE, Save/Load ve performans doğrulaması çalıştırılmadı.

## Tam kabul için zorunlu sıra

1. Yeni dalı yerel kopyada al; UE 5.8 UHT + Development Editor derlemesini çalıştır ve tüm hataları düzelt.
2. `MyProject3.Terrain.Voxel.*` testlerini ve var olan placement / ownership / structural graph / save coordinator testlerini çalıştır.
3. Gerçek PIE'de kütük taşıma-kesme-snap-yapı, sınırlı kazı, sert/yumuşak zemin, destek altı kazı reddi ve fiziksel çarpışmayı kontrol et.
4. Save → dig/build/farm/raid → save → world mutate → load → identity/count/graph/collision doğrulaması; rollback ve eski şema testleri.
5. Profiling ile 16,7 ms kare bütçesi, collision cook, chunk sınırı, bellekteki yoğunluk ve uzun oturum sızıntılarını ölç.
6. Her M001–M119 için test istasyonu, pozitif/negatif senaryo, başlangıç/temizlik adımı ve kanıt kaydını tamamla; ancak sonra fazları kabul edilmiş olarak işaretle.