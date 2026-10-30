# Elimizde ne var

**Son kontrol:** 2026-10-30 · kod dalı `game/p00-adopt-group-a` · commit `6ca8e9c` · Unreal 5.8 derlemesi başarılı · **136/136 otomatik test geçiyor** (Claude Code ile çalıştırıldı) · oyunda (PIE) hiçbiri denenmedi.

Kısaca: ilkel çağın arka plan sistemlerinin (Faz 0–5) hepsi ve Faz 6–9'un bir kısmı kodda var, birbirine bağlı ve testli. **Oynanışa dökülmedi:** yeni oyun sahnesi yok, oyuncu tuşlarının bir kısmı var, ekran arayüzü basit.

---

## 1. Kod ve veri boyutu

- C++: 68 başlık dosyası, yaklaşık 50 bin satır (`Source/MyProject3/`).
- Testler: 136 (`Tests/MyProject3CoreTests.cpp` 76, `Tests/GameWorldTests.cpp` 40, `Tests/PlacementKernelTests.cpp` 20).
- Veri: `Config/GameData/` içinde 15 JSON dosyası (aşağıda).
- Kayıt (save) sürümü: 31.

## 2. Sistemler

| Alan | Kodda olan | Ana dosyalar | Eksik |
|---|---|---|---|
| **Ortak dünya** | Eşya sahipliği defteri (eşya tek yerde: dünya/el/envanter/depo/yolda/tüketildi), işlem makbuzları, sabit adımlı simülasyon koordinatörü ve geri alma, kayıt/yükleme | `WorldItemOwnershipSubsystem`, `WorldSimulationCoordinatorSubsystem`, `WorldSaveCoordinatorSubsystem` | Fizikten sonra aktör konumlarının tam kaydı |
| **Veri** | Bütün listeleri yükleyen ve her referansı kontrol eden yükleyici; envantere ve ateşe veriyi kendiliğinden verir | `WorldGameDataSubsystem` | — |
| **Takvim, iklim** | Yıl/ay/gün/saat/mevsim (ay = 5 gün); aylık iklim, günlük hava, sıcaklık eğrisi, kar kalınlığı, zemin ıslaklığı, yağış sayacı | `WorldCalendar`, `WorldClimateSubsystem` | — |
| **Doğal olaylar** | 11 olay (kar fırtınası, sağanak, sel, çığ, dolu, kuraklık, orman yangını, zehirli yağmur, salgın, sis, geç don); işaret → etki aşamaları; ay ve sıcaklık koşulları; zincirler (yangın→zehirli yağmur, sel→salgın); hazırlık eşyası şiddeti düşürür; etkiler: soğuk, ıslaklık, yol kapanması, yapı hasarı, ekin hasarı, yangın yayılması, hastalık | `WorldEventDirectorSubsystem` | Sisin dünya etkisi yok (yalnızca işaret) |
| **Ekran verisi** | Tarih, mevsim, hava, sıcaklık, olay bildirimleri, bir sonraki olaya geri sayım | `WorldHudDataSubsystem`, `LogSnapHUD` | Görsel arayüz basit (Canvas yazısı) |
| **Hayatta kalma** | Açlık (kalori), susuzluk, yorgunluk, dayanıklılık, vücut ısısı, ıslaklık, hastalık/yara/zehirlenme ve tedavi, dinlenme (yer kalitesi), giysi yuvaları; yorgunluk yürüyüşü yavaşlatır | `WorldSurvivalSubsystem`, `PhysicalHealthComponent` | Denge |
| **Yiyecek** | Tazelik; bozulma depoya (kiler, tütsü rafı) ve sıcaklığa göre; kirli su ve bozuk yiyecek hastalık yapar; yapılan yiyecek yenebilir | `WorldSurvivalSubsystem`, `WorldItemOwnershipSubsystem` | — |
| **Ateş, barınak** | Yakıt eşyaları (odun, ıslak odun, kömür) ısıya göre; yağmur açık ateşi söndürür, açıktaki odunu ıslatır, çatı altındakini değil; barınak ışınla ölçülür, kapalı barınak ateş ısısını tutar; sıva boşlukları kapatır | `HeatSourceComponent`, `ShelterSensorComponent`, `WallCoatingComponent` | — |
| **Üretim** | 38 plan dönüşümü (61 satır): elle tarifler (alet şartlı, alet harcanmaz) ve istasyon tarifleri; 10 istasyon; ateş/kömür ateşi şartı, ateş yoksa iş bekler; toplama noktaları (alet, zamanla dolma) | `InventoryCraftingComponent`, `WorldProductionSubsystem`, `ProductionStationActor`, `GatherableResourceActor` | İstasyon görselleri |
| **Kütük, inşa (snap)** | Kütük kesme/yarma/yontma, taşıma, nişana göre yerleştirme, simetrik çaprazlar, tek yerleştirme çekirdeği; taş parçalar ve taş köprüler; yapı grafiği (destek, hasar, çökme), onarım ve söküm (malzemeyle) | `LogBuildPiece`, `LogCarryComponent`, `Placement/*`, `StoneBuildingPiece`, `WorldStructuralGraphSubsystem` | Fiziksel montaj ayrıntıları (Faz 10, donduruldu): 45° kesim, çentik, saz demeti |
| **Kazı** | Voxel kazı (sınırlı), bölge/derinlik/tohuma göre jeoloji, kazıdan eşya, kiler, sarnıç (yağmur suyu), hendek, set, desteksiz odanın çökmesi | `VoxelTerrainChunkComponent`, `WorldTerrainStateSubsystem`, `TerrainDigToolComponent`, `EarthworkActor` | Parça sınırları (komşu bölümler) |
| **Tarım, hayvan** | Tarla hücresi (temiz/işli/verim/ot/malç/nem), tarla işlemleri, 3 ekin (ekim ayları, toprak yorgunluğu, tohum geri dönüşü), hasat; hayvan yakalama, ağıl, besleme, ürün, kesim (et/deri/kemik) | `WorldAgricultureSubsystem`, `WildlifeActor` | 15 adımlık zincirin bir kısmı, denge |
| **Hareket, keşif** | Yük ağırlığı yürüyüşü yavaşlatır; eğim; 5 bölge; 6 keşif (önkoşul, ekipman, ödül); yol grafiği ve taşıma maliyeti | `CarryLoadComponent`, `WorldLogisticsRouteSubsystem`, `WorldCargoTransportSubsystem` | Yürüyüş yol grafiğiyle aynı maliyeti kullanmıyor; altyapı türleri (patika, merdiven, sal, ileri kamp) yok |
| **Tehditler** | Baskın yöneticisi (uyarı → aktif), baskın baskısı işaretlerden (depo, ışık, duman), yapı hasarı | `WorldThreatDirectorSubsystem`, `WorldRaidAdapterSubsystem` | Baskıncıların dünyada belirmesi ve hareketi, kar yükü, kırılan parçanın fiziksel yıkılması |
| **Yardımcı NPC** | Takip, kütük toplama, yığma, plan kurma, duvar bakımı | `HelperCompanionActor`, `BuildPlanActor` | İstasyon kullanma |

## 3. Veri dosyaları (`Config/GameData/`)

| Dosya | Adet |
|---|---|
| items.json | 89 eşya |
| transformations.json | 61 satır (planın 38 dönüşümü + tohum ayırma) |
| stations.json | 10 istasyon |
| events.json | 11 olay |
| climate.json | 12 ay |
| calendar.json | ay 5 gün, başlangıç 5 Mart 08:00 |
| conditions.json | 8 hastalık/yara |
| crops.json | 3 ekin (geçici seçim) |
| animals.json | 2 hayvan |
| zones.json | 5 bölge |
| discoveries.json | 6 keşif |
| field_actions.json | 7 tarla işlemi |
| layers.json | 6 kazı katmanı |
| materials.json | 10 malzeme grubu |
| structure_repairs.json | 3 onarım |

## 4. Oyuncu tuşları (kodda tanımlı)

E etkileşim · F parçayı işle (kes/yar) · C kesim seç · R döndür · G bırak · Q fırlat · V kes/it-yuvarla · B sürükle · X sivrilt/kaz · N kaz · Z doldur/plan seç · Y plan yerleştir/bırak · H ip/ateşe bak · J yardımcı emri/ip geçişi · K kapı · L kilit · P pencere · U tepe · T al/çek · I dereden su doldur · Delete sök · F8 sıva · F1 yardım · F2–F7, F9, F10 istasyon ve hayatta kalma kısayolları (içme M, kaynatma F5, dinlenme F6, durum F7, yakıt O).

**Dikkat:** Birkaç tuş iki bileşende birden tanımlı (B, H, J, T, V, X, Y, Z). Oynanışa dökerken çakışmalar ayıklanmalı.

## 5. Sahneler (haritalar)

- `/Game/DemoAssets/Maps/VerticalSlice_C`: ana test sahnesi, kodla kuruluyor (`VerticalSliceCWorldActor`). Simülasyon koordinatörünü çalıştırıyor; birkaç istasyon ve toplama noktası var.
- `VerticalSlice_A`, `VerticalSlice_B`, `VerticalSlice_B_Construction`, `Content/Maps/LogSnap_Playground` ve iki test haritası.
- **Yeni oyun sahnesi ("Vadi") yok.** Planlandı, en sona bırakıldı.

## 6. Doğrulama seviyesi

| Seviye | Durum |
|---|---|
| Derleniyor | Evet (2026-10-30) |
| Otomatik testler | 136/136 |
| Oyunda denendi (PIE) | Hayır |
| Bir oyun yılı uçtan uca | Hayır |

Not: `memory/PLAN_STATUS.md` içindeki bazı satırlarda hâlâ "unbuilt" yazıyor; o satırlar eskidi. 2026-10-30 itibarıyla kod dalının tamamı derleniyor ve testler geçiyor.

## 7. Dallar ve etiketler

- `game/p00-adopt-group-a`: **tek kod dalı.**
- `main`: geride; birleştirme kararı kullanıcıda.
- `game/p06-p08-blind-code-closure`: eski bir noktadan ayrılmış ChatGPT dalı. İşe yarar parçaları (altyapı aktörü, yanıcı malzeme bileşeni, altyapı/bilgi/baskın savunması/yol verileri) henüz taşınmadı. Bu dala yazılmaz.
- `analysis`: bu dal.
- `archive/*` etiketleri: silinen 13 eski dalın son hâli (geri getirilebilir).

## 8. Sıradaki doğal adım

Arka plan büyük ölçüde hazır. Sıradaki iş **oynanışa dökmek**: yeni sahneyi kodla ve basit geometriyle kurmak, tuşları ayıklamak, ekran arayüzünü yapmak, sonra oynayıp dengelemek.
