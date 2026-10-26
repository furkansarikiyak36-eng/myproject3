# MyProject3 — 119 Mekanik: Tek Ana Belge

Tarih: 26 Ekim 2026
Kaynak kayıt: `Docs/Architecture/MECHANIC_REGISTRY_119.csv` (ID, ad, sahip ve önkoşullar değiştirilmedi).
Kod taban: `origin/demo/vertical-slice-phase-01` (d1ebd78).
Bağlı görev belgesi: [REFERENCE_GAMES_DEEP_TASKS.md](REFERENCE_GAMES_DEEP_TASKS.md) (SOTF-, 7D-, DS-, FS-, SN-, RISK-, NET- görevleri). ITEM-, TXN-, SAVE-, CLOCK-, TEST-, REG-, TERR- görevleri "Tüm Açık Eksikler" listesindendir.

## 0. Bu belge nasıl okunur

Her mekanik için:

- **Tanım:** Mekaniğin oyunda neyi sağladığı ve "bitti" sayılması için gözlenecek sonuç (pozitif + negatif).
- **Referans:** Mekaniğin hangi referans oyunun çekirdeğinden beslendiği.
- **Kod:** Statik kaynak incelemesine göre durum ve kanıt (sınıf/fonksiyon adı).
- **Açık:** Kabul için eksik olanlar.
- **Görevler:** Açığı kapatan görev ID'leri.

### Kod durumu etiketleri

| Etiket | Anlamı |
|---|---|
| **Yok** | Kaynakta karşılığı yok. |
| **Enum** | Yalnız enum/veri alanı var; davranış yok. |
| **S1** | Kaynak katmanı: tek sistemde C++ mantığı var; oyuncu akışı ve diğer sistemlerle bağlantı eksik. |
| **S2** | Entegre kaynak: birden fazla sistem birbirine bağlı (sahiplik, graph, kayıt gibi). |
| **S4** | Doğrulandı: UE 5.8 derlemesi + çalışma zamanı kanıtı. **Şu an hiçbir mekanik S4 değildir.** |

Önemli: #30 dalı UHT aşamasında `HarvestableTreeActor.h:100` hatası nedeniyle derlenmiyor (düzeltme yalnız yerel kopyada denendi, UHT geçti, C++ derlemesi tamamlanmadı). Bu yüzden S1/S2 etiketleri "kod yazılmış" demektir, "çalışıyor" değil. Kayıttaki resmi alanlar (`StaticReview`, `AutomatedTest`, `UnrealBuild`, `RuntimeTest`) 119/119 `not_run` olarak kalır; bu belge onları değiştirmez.

### Özet

| Sistem | Mekanikler | Yok/Enum | S1 | S2 |
|---|---|---|---|---|
| Çekirdek dünya sözleşmeleri | M001–M008 | 0 | 4 | 4 |
| Kütük ve ahşap işleme | M009–M028 | 4 | 9 | 7 |
| Taş ve duvarcılık | M029–M038 | 4 | 6 | 0 |
| Çubuk ve ilkel aletler | M039–M048 | 1 | 9 | 0 |
| Envanter ve üretim | M049–M058 | 2 | 5 | 3 |
| Ortak yapı ve hasar | M059–M070 | 5 | 4 | 3 |
| Arazi, dünya durumu, kazı | M071–M080 | 6 | 4 | 0 |
| Tarım ve hayvancılık | M081–M090 | 3 | 6 | 1 |
| Baskın ve tehdit AI | M091–M100 | 4 | 4 | 2 |
| Lojistik, üretim, survival, keşif | M101–M119 | 4 | 13 | 2 |
| **Toplam** | **119** | **33** | **64** | **22** |

---

## 1. Çekirdek dünya sözleşmeleri (M001–M008)

#### M001 — Fiziksel dünya nesneleri için kalıcı kimlik
- **Tanım:** Her fiziksel nesnenin (kütük, taş, çubuk, yapı parçası, item) kayıt/yükleme ve sahiplik değişimi boyunca değişmeyen bir kimliği vardır. Yinelenen kimlikle yükleme reddedilir.
- **Referans:** Tümü (SOTF fiziksel malzeme sütunu).
- **Kod:** S2 — `UWorldObjectIdentityComponent`, `UWorldObjectIdentitySubsystem`; kütük/taş/çubuk aktörlerinde `StructuralIdentity`; sahiplik ve kayıt bu kimliği kullanıyor.
- **Açık:** Envanterdeki/depodaki item ile dünyadaki aktörün aynı kimliği taşıma kuralı; yinelenen kimlikle yükleme testi.
- **Görevler:** ITEM-02, SAVE-05, SOTF-M01.

#### M002 — Nesnenin tek ve dışlayıcı yaşam döngüsü durumu
- **Tanım:** Bir nesne aynı anda yalnız bir durumdadır: dünyada, elde, envanterde, depoda, taşımada, üretime ayrılmış, tüketilmiş, yok edilmiş. Geçersiz geçiş reddedilir.
- **Referans:** Tümü.
- **Kod:** S1 — aktörlerde `bIsHeld`/`bIsPlaced` bayrakları, `WorldItemOwnershipSubsystem` sahip türü; tek durum makinesi yok.
- **Açık:** Tüm durumları kapsayan tek durum makinesi; bayrak çiftlerinin çelişmesini engelleyen kural.
- **Görevler:** ITEM-03, SOTF-M01.

#### M003 — Dünyayı değiştiren atomik işlem
- **Tanım:** Dünyayı değiştiren her işlem ya tamamen uygulanır ya hiç uygulanmaz; aynı işlem kimliğiyle tekrar ikinci sonuç üretmez.
- **Referans:** Tümü (özellikle Hydroneer/Satisfactory üretim ağı, FS hasat).
- **Kod:** S2 — `WorldOperationLedgerSubsystem::TryBeginOperation/TryGetCompletedResult`, atomik kütük bölme (#23), koordinatör rollback'i (#18), crafting/drop rollback (#28, #29).
- **Açık:** Çok aktörlü işlemler (graph + aktör yok etme + söküm) tek işlem değil; rollback'in kendisi başarısız olursa dünya "fault" durumuna geçme kuralı her yerde yok.
- **Görevler:** TXN-01..TXN-09.

#### M004 — Yapılandırılmış işlem sonucu ve hata sebebi
- **Tanım:** Her işlem başarı/başarısızlık yanında makine ve oyuncu tarafından okunabilir bir sebep kodu döndürür.
- **Referans:** Satisfactory (engel sebepleri), SOTF (yerleştirme reddi).
- **Kod:** S1 — `ALogBuildPiece::GetCutActionBlockReason`, `GameplayCoreTypes.h` sonuç türleri; birçok API hâlâ yalnız `bool` döndürüyor.
- **Açık:** Tüm dünya API'lerinde ortak sonuç yapısı; UI metni eşlemesi.
- **Görevler:** TXN-01, SOTF-B03, NET-R04.

#### M005 — Commit edilmiş alan olayları
- **Tanım:** Olaylar yalnız durum commit edildikten sonra yayınlanır; başarısız işlem olay üretmez; tekrar oynatmada çift olay yoktur.
- **Referans:** Tümü.
- **Kod:** S1 — `WorldCommittedEventSubsystem::PublishCommittedEvent`.
- **Açık:** Tekilleştirme ve yeniden oynatma sözleşmesi; olayları tüketen sistemler az.
- **Görevler:** TXN-05.

#### M006 — Deterministik dünya saati ve rastgele seed
- **Tanım:** Tüm zamanlı sistemler tek saatten ilerler; rastgelelik ortak seed + işlem alt anahtarından üretilir; kare hızı sonucu değiştirmez.
- **Referans:** Frostpunk/TLD (zaman baskısı), FS (mevsim), 7D (baskın takvimi).
- **Kod:** S2 — `WorldSimulationClockSubsystem::AdvanceSimulation`, `GetDeterministicSeedForOperation`; koordinatör tüm alt sistemleri aynı adımla ilerletiyor.
- **Açık:** Takvim/mevsim katmanı yok; uyku ile zaman atlama sözleşmesi yok.
- **Görevler:** CLOCK-01..CLOCK-07, FS-G02, RISK-B04.

#### M007 — Sürümlü kayıt/yükleme ve göç sınırı
- **Tanım:** Kayıt şeması sürümlüdür; eski kayıtlar tanımlı göç yoluyla açılır; tutarsız kayıt oyunu açmaz.
- **Referans:** Tümü (DS kalıcı değişim sütunu).
- **Kod:** S2 — `WorldSaveCoordinatorSubsystem::SaveWorldState/LoadWorldState/ApplySnapshot`, `WorldSimulationSaveGame`; fail-closed rollback (#27).
- **Açık:** Yükleme, aynı aktör setinin sahnede var olmasını bekliyor; çalışma zamanında doğmuş aktörlerin yeniden yaratılması yok; göç yolu tek yerde tanımlı değil.
- **Görevler:** SAVE-01..SAVE-12.

#### M008 — Ortak survival durumu: sağlık, stamina, sıcaklık ve ihtiyaçlar
- **Tanım:** Oyuncu, yardımcı ve yerleşimciler aynı survival profilini kullanır; değerler yalnız eylemler, çevre ve zamanla değişir.
- **Referans:** TLD, Frostpunk.
- **Kod:** S1 — `FWorldSurvivalProfile` (Health, Stamina, Hunger, Thirst, Fatigue, Wetness, ClothingInsulation, ShelterProtection), `AdvanceSurvivalStep`.
- **Açık:** Vücut sıcaklığı/ısı deposu ayrı değil; kalori yerine yüzde; giysi ve barınak serbest sayı olarak set ediliyor.
- **Görevler:** RISK-B01..B03, RISK-H01.

---

## 2. Kütük ve ahşap işleme (M009–M028)

#### M009 — Ağacı alet vuruşlarıyla devirmek
- **Tanım:** Ağaç belirli sayıda alet vuruşuyla devrilir; devrilme vuruş yönüne göredir; devrilen gövde yolundaki yapıya hasar verir; zemine oturup durmadan kütük üretilmez.
- **Referans:** SOTF.
- **Kod:** S2 — `AHarvestableTreeActor::ApplyChopDamage`, `CanBeChopped`, `HasGroundContact`, kayıtta tutulan `StableGroundedSeconds`.
- **Açık:** Vuruş yönüne göre devrilme, devrilme hasarı, alet kalitesinin vuruş sayısına etkisi.
- **Görevler:** SOTF-T01, SOTF-T02, DS-X07.

#### M010 — Ağaç boyutundan kütük üretmek
- **Tanım:** Ağacın boyu ve çapı, çıkacak kütüklerin sayısını, uzunluğunu ve çap bandını belirler; dallar çubuk, kabuk ve reçine yan ürün verir.
- **Referans:** SOTF, FS ormancılık, Hydroneer/Satisfactory üretim ağı.
- **Kod:** S1 — `AHarvestableTreeActor::SpawnHarvestLogs`; veri tablosuna bağlı değil, dal/kabuk yok.
- **Açık:** Ağaç data asset'i, dal → çubuk, yan ürünler, kısmi doğuş hatasında geri alma.
- **Görevler:** SOTF-T01, SOTF-T04, SOTF-L13.

#### M011 — Gevşek kütüğü kaldırıp taşımak
- **Tanım:** Oyuncu kütüğü omzuna alır (sınırlı adet); kütle hızı ve staminayı etkiler; sahiplik `Held` olur.
- **Referans:** SOTF, DS (yük).
- **Kod:** S2 — `ULogCarryComponent::TryPickupTarget`, `ALogBuildPiece::TryTransferOwnership`, sahiplik defteri.
- **Açık:** 2 kütük kuralı, kütleye göre hız/stamina, aynı bileşenin çubuk ve taşı da taşıması.
- **Görevler:** SOTF-C01, DS-C02.

#### M012 — Taşınan kütüğü bırakmak veya hızlı bırakmak
- **Tanım:** Bırakılan kütük fizikle düşer, sahipliği `World` olur; fizik aktivasyonu başarısızsa sahiplik geri alınır.
- **Referans:** SOTF.
- **Kod:** S2 — `ULogCarryComponent::DropHeldPiece`; fiziksel bırakma fail-closed (#29).
- **Açık:** Fırlatma (ivmeli bırakma) ve çarpma hasarı.
- **Görevler:** SOTF-C01, M014 görevleri.

#### M013 — Gevşek kütüğü yuvarlamak, sürüklemek ve itmek
- **Tanım:** Yerdeki kütük itilir, yuvarlanır veya sürüklenir; eğim ve kütle hız ve staminayı etkiler; elde, yerleşik veya bağlı kütük bu etkileşimi kabul etmez.
- **Referans:** SOTF, DS (arazi maliyeti).
- **Kod:** S1 — `ULogCarryComponent::TryPushOrRollAimedLog`, `BeginDragAimedLog/EndDragAimedLog`.
- **Açık:** Eğim ve kütle maliyeti, stamina bağlantısı, yerleşik kütükte red testi.
- **Görevler:** SOTF-C01, DS-P02, RISK-B01.

#### M014 — Kütük çarpışması ve darbe hasarı
- **Tanım:** Fırlatılan, yuvarlanan, düşen veya çöken kütük çarptığı yapıya, karaktere veya hayvana kütle ve hızla orantılı hasar verir; hasar tek otorite (`ApplyDamage`) üzerinden geçer.
- **Referans:** SOTF, 7D (düşen kütük tuzağı).
- **Kod:** Yok — `LogCarryComponent.cpp` içindeki `ImpactPoint` yalnız yerleştirme önizlemesi; darbe hasarı yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-T02, SOTF-D02, 7D-R09.

#### M015 — Kütüğü geçerli bir uzunlukta enine kesmek
- **Tanım:** Vurulan noktaya göre izinli kesim noktasında enine kesim; çıkan parçaların uzunluk toplamı kaynağa eşit; mesh'i olmayan uzunluk reddedilir; kesim ilerlemesi vuruşla artar.
- **Referans:** SOTF, FS ormancılık.
- **Kod:** S1 — `ALogBuildPiece::TryApplyCutAction`, `TryAdvanceCutProgress`; boyuna/uzunluk kesme işinin büyük kısmı PR'sız `feat/atomic-log-length-splitting` dalında (#25 hattı).
- **Açık:** #24/#25 kanonik model kararı; 3/4 (450 cm mesh yok) ve 75 cm (enum yok) tutarsızlığı; vuruş noktası.
- **Görevler:** REL-03, SOTF-L01, SOTF-L02, SOTF-L04, SOTF-L05.

#### M016 — Tam kütüğü boyuna yarmak
- **Tanım:** Tam yuvarlak kütük iki yarım kesit parçaya ayrılır; ikinci parça benzersiz kimlik ve sahiplik alır; tekrar ikinci kopya üretmez.
- **Referans:** SOTF.
- **Kod:** S2 — `ELogCutAction::SplitHalf`, `ALogBuildPiece::TrySplitOutputPieces`, sahiplik bölme `WorldItemOwnershipSubsystem::TrySplitItem` (#23).
- **Açık:** İkinci parçanın konum/dönüşünün ölçülmüş mesh'e dayanması; FBX manifesti.
- **Görevler:** SOTF-M05, SOTF-L01.

#### M017 — Yarım kesiti çeyreklemek
- **Tanım:** Yarım kesit iki çeyrek kesite ayrılır; çeyrek kesit ile çeyrek uzunluk karışmaz.
- **Referans:** SOTF.
- **Kod:** S2 — `ELogCutAction::Quarter`, `ELogPieceType::QuarterSectionLog` (#22 anlam ayrımı).
- **Açık:** `*_Quarter_04` mesh adının anlamı; manifest.
- **Görevler:** SOTF-M05.

#### M018 — Kütüğü yontmak veya kare kesite getirmek
- **Tanım:** Yuvarlak → 1 yüz → 2 yüz → kare kiriş (kalas) zinciri; adım atlanamaz; talaş yan ürün.
- **Referans:** SOTF, üretim ağı.
- **Kod:** S1 — `ELogCutAction::Hewn`, `Kalas`; `Hewn1`/`Hewn2` mesh'leri var ama enum tek değer.
- **Açık:** `HewnTwoFaces` durumu, zincir kuralı, talaş.
- **Görevler:** SOTF-L03, SOTF-L13.

#### M019 — Oluğu yalnız yarmadan sonra açmak
- **Tanım:** Oluk yalnız yarım kesite açılır; tam yuvarlakta istek hiçbir şeyi değiştirmeden reddedilir. Oluk su taşıma ve ayıklama düzeneğinde kullanılır.
- **Referans:** SOTF, Hydroneer (oluk ayıklama).
- **Kod:** S1 — `ELogCutAction::Gutter`, `CanApplyCutAction`/`GetCutActionBlockReason` önkoşul denetimi.
- **Açık:** Negatif durumun testle kanıtı; oluğun su/ayıklama işlevi.
- **Görevler:** SOTF-L03, NET-D03, NET-W01, TEST-03.

#### M020 — Yalak veya kanal yapmak
- **Tanım:** Kütük oyularak yalak olur; yalak su ve yem tutar (hayvan suluğu, ıslatma havuzu); kanal suyu taşır.
- **Referans:** SOTF, FS (suluk), Hydroneer (su).
- **Kod:** S1 — `ELogCutAction::Trough`, `*_Trough` mesh'leri; işlev yok.
- **Açık:** Su/yem depolama bileşeni, su ağına bağlantı.
- **Görevler:** FS-A02, NET-S07, NET-W01.

#### M021 — Yarılmış kütükten plank üretmek
- **Tanım:** Kalastan veya yarım kesitten çap bandına göre tanımlı sayıda plank; hacim dengesi korunur.
- **Referans:** SOTF, üretim ağı.
- **Kod:** S1 — `ELogCutAction::Plank`, `*_Plank` mesh'leri; verim sayısı tanımsız.
- **Açık:** Verim tablosu, hacim dengesi testi.
- **Görevler:** SOTF-L11, ITEM-06.

#### M022 — Çentik ve geçme birleşimleri açmak
- **Tanım:** Düz, eyer ve bindirme çentikleri; çentikli kütükler köşede kilitlenir; çentiksiz kütük yalnız yaslanır.
- **Referans:** SOTF.
- **Kod:** Enum — `NotchedLog`, `JointPiece`; tek mesh `SM_RawLog_75_FlatNotch_TopBottom`.
- **Açık:** Çentik eylemleri, mesh'ler, köşe kilidi kuralı.
- **Görevler:** SOTF-L08.

#### M023 — Zemine çakılmış sivri uç (kazık) yontmak
- **Tanım:** Kütüğün bir ucu sivriltilir; dikili sivri kütük savunma hattı ve tuzak olarak temas hasarı verir.
- **Referans:** SOTF (savunma duvarı), 7D (tuzak).
- **Kod:** Yok — kütük için sivriltme eylemi yok (çubukta `Sharpened` var).
- **Açık:** Tamamı.
- **Görevler:** SOTF-L06, 7D-R09.

#### M024 — Snap anchor üretmek ve doğrulamak
- **Tanım:** Parçalar geometriden türetilen bağlantı noktalarına sahiptir; noktaların doluluğu ve uyumu doğrulanır.
- **Referans:** SOTF.
- **Kod:** S2 — `main` üzerinde otomatik yan yüz soketleri; `ALogBuildPiece::TryReserveSnapPoints`, `CanSnapPointAccept`, `ReleaseSnapPoint`.
- **Açık:** Malzemeler arası anchor uyumluluğu (kütük–taş–çubuk).
- **Görevler:** SOTF-M02.

#### M025 — Kütüğü yatay, dikey ve çapraz yerleştirmek
- **Tanım:** Üç yerleştirme modu ve her biri için ayrı zemin/destek kuralı; önizleme ile commit aynı hesaplayıcı.
- **Referans:** SOTF.
- **Kod:** S1 — `ELogPlacementMode`, `ULogCarryComponent::PlaceHeldPiece`, `FindBestSnapCandidate`.
- **Açık:** Mod başına zemin kuralı, önizleme = commit garantisi, red sebebi UI.
- **Görevler:** SOTF-B01, SOTF-B03.

#### M026 — Açık ebeveyn–çocuk yapı bağlantıları
- **Tanım:** Her bağlantı açıkça kaydedilir; kayıttan sonra aktör üzerindeki bağlantı graph'tan yeniden kurulur.
- **Referans:** SOTF.
- **Kod:** S2 — `AttachToSnapParent`, `DetachFromSnapParent`, `RestoreActorLocalSnapLinkFromGraph`, `WorldStructuralGraphSubsystem::TryConnect`.
- **Açık:** Tek ebeveyn modeli; çoklu destek gerekiyor.
- **Görevler:** SOTF-D01, STRUCT-02.

#### M027 — Destek kaldırmak ve çökmeyi çözmek
- **Tanım:** Destek kaldırılınca bağımlı parçalar tanımlı sırayla düşer; düşen parçalar fiziksel ve toplanabilir olur; graph'ta yetim düğüm kalmaz.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — `CanSafelyRemove`, `CollectDependentsDepthFirst` (planlama); fiziksel çökme yok.
- **Açık:** Fiziksel çökme, çöken parçanın sahipliği, deterministik sıra.
- **Görevler:** SOTF-D02, 7D-S03.

#### M028 — Ahşabı onarmak, sökmek ve yakmak
- **Tanım:** Hasarlı parça kendi malzemesiyle onarılır; sökülen parça durumuna göre item olarak döner; ahşap yakıt olarak yanar.
- **Referans:** SOTF, 7D (onarım), TLD/Frostpunk (yakıt).
- **Kod:** Yok — onarım, söküm ve yakma davranışı yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-S05, SOTF-D03, RISK-F01, SOTF-L07.

---

## 3. Taş ve duvarcılık (M029–M038)

#### M029 — Gevşek taşı toplamak, tutmak ve fırlatmak
- **Tanım:** Taş boyut bandına göre taşınır (çakıl kapta, el taşı elde, büyük taş iki elle, kaya taşınamaz); el taşı fırlatılabilir ve kütleyle orantılı hasar verir.
- **Referans:** SOTF.
- **Kod:** S1 — `AStoneBuildingPiece` (`bCanBeCarried`, `bIsHeld`); ölçü/kütle/tür alanı yok; fırlatma yok; taşıma bileşeni yalnız kütük için.
- **Açık:** Boyut bandı, kütle, fırlatma, ortak taşıma bileşeni.
- **Görevler:** SOTF-S01, SOTF-S02, SOTF-C01.

#### M030 — Ham taşı yapı parçasına şekillendirmek
- **Tanım:** Alet + girdi + süre ile ham taştan blok, levha, köşe, sütun, lento, basamak; çakmaktaşı yonga çıkarma ayrı eylem ailesi; geçersiz işlem hiçbir şeyi değiştirmez.
- **Referans:** SOTF, üretim ağı.
- **Kod:** Enum — `EStonePieceType {RawStone, Block, Slab, Column, Lintel, Corner, Other}`; taşta işleme fonksiyonu yok.
- **Açık:** Eylem grafiği, alet şartı, yan ürün (çakıl), mesh'ler.
- **Görevler:** SOTF-S03, SOTF-M06.

#### M031 — Tek tek taş yerleştirmek ve hizalamak
- **Tanım:** Taş zemin/yüzeye oturma noktalarına göre yerleşir; yön, doluluk ve tolerans denetlenir; çakışma tek başına bağlantı kurmaz.
- **Referans:** SOTF.
- **Kod:** S1 — `AStoneBuildingPiece::TryCommitGroundPlacement`, `FStoneSnapAnchor`.
- **Açık:** Oturma yüzeyi mantığı, önizleme.
- **Görevler:** SOTF-S04, SOTF-B03.

#### M032 — Taşları yatay ve dikey istiflemek
- **Tanım:** Taşlar üst üste ve yan yana dizilir; derz kaydırma (iki alt taşa basma) bağlantıyı güçlendirir.
- **Referans:** SOTF.
- **Kod:** S1 — `AStoneBuildingPiece::TryConnectToAnchor/CanConnectToAnchor` (yalnız taş↔taş).
- **Açık:** Derz kuralı, bağlantı gücü.
- **Görevler:** SOTF-S04.

#### M033 — Taş duvar ve köşe örmek
- **Tanım:** Kuru veya harçlı duvar; köşede iki duvar birbirine kilitlenir.
- **Referans:** SOTF.
- **Kod:** Enum — `EStonePieceType::Corner`; duvar/köşe deseni yok.
- **Açık:** Duvar ve köşe birleşim kuralı, harç.
- **Görevler:** SOTF-S04, SOTF-S07.

#### M034 — Taş sütun, kiriş ve açıklık yapmak
- **Tanım:** Sütun dikey yük taşır; lento veya kemer açıklığı geçer; kemer tamamlanmadan geçici destek ister.
- **Referans:** SOTF.
- **Kod:** Enum — `Column`, `Lintel`.
- **Açık:** Davranış, açıklık limiti, kemer.
- **Görevler:** SOTF-S11, M064 görevleri.

#### M035 — Taş desteğini, dengesini ve çökmesini çözmek
- **Tanım:** Taş yapının yükü malzeme profiline göre hesaplanır; hizalı kolon derzli duvardan önce çöker; çöken taş fiziksel olur.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — graph düzeyinde `EStructuralNodeKind::Stone` ve yük alanları; taşa özel denge kuralı ve fiziksel çökme yok.
- **Açık:** Malzeme yük profili, derz etkisi, fiziksel çökme.
- **Görevler:** 7D-S01..S03, SOTF-S04.

#### M036 — Taş yapıyı hasarlandırmak, onarmak ve sökmek
- **Tanım:** Hasar tek otoriteden; onarım taş ve harçla; söküm taşı durumuna göre geri verir.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — genel `WorldStructuralGraphSubsystem::ApplyDamage`; onarım/söküm yok.
- **Açık:** Onarım, söküm, ganimet.
- **Görevler:** 7D-S05, SOTF-D03.

#### M037 — Taşı toplu depolamak ve taşımak
- **Tanım:** Taş tutucu ve çakıl kabı; kızak taş yuvası; görsel doluluk yetkili stokla aynı.
- **Referans:** SOTF.
- **Kod:** S1 — genel `WorldStorageSubsystem`/`WorldStorageComponent`; taş tutucu ve kızak yok.
- **Açık:** Malzemeye özel tutucu, kızak, görsel doluluk.
- **Görevler:** SOTF-C02, SOTF-C03, SOTF-S10.

#### M038 — Karma ahşap–taş yapı ve taş savunmalar
- **Tanım:** Taş temel üstüne kütük duvar, taş ocak + kütük baca kirişi, taş ayaklı köprü; taş duvar savunması.
- **Referans:** SOTF, DS (köprü ayağı), 7D (savunma).
- **Kod:** Yok — `TryConnectToAnchor` türe kilitli (taş yalnız taşa, çubuk yalnız çubuğa bağlanıyor); aktör düzeyinde karma bağlantı imkânsız.
- **Açık:** Malzemeler arası anchor tablosu ve ortak parça arayüzü.
- **Görevler:** SOTF-M01, SOTF-M02, SOTF-S08, DS-X02.

---

## 4. Çubuk ve ilkel aletler (M039–M048)

#### M039 — Çubuk toplamak ve kırmak
- **Tanım:** Çubuk yerden (kuru), çalıdan (yaş) ve devrilen ağacın dallarından toplanır; demet hâlinde taşınır; kırılınca uzunluk toplamı korunur.
- **Referans:** SOTF.
- **Kod:** S1 — `AStickPrimitivePiece`, `EStickPrimitiveState::Raw/Broken`, `TryProcessTo`; uzunluk/kalınlık/nem yok; demet taşıma yok.
- **Açık:** Kaynaklar, varyant eksenleri, demet.
- **Görevler:** SOTF-K01, SOTF-K02, SOTF-K03, SOTF-T04.

#### M040 — Çubuğu sivriltmek veya çift uçlu kazık yapmak
- **Tanım:** Bıçak/balta ile tek veya çift sivri uç; ateşte sertleştirme dayanımı artırır.
- **Referans:** SOTF.
- **Kod:** S1 — `Sharpened`, `DoubleEndedStake` durumları.
- **Açık:** Ateşte sertleştirme, alet şartı.
- **Görevler:** SOTF-K03.

#### M041 — İlkel alet ve yakın dövüş silahı yapmak
- **Tanım:** Çubuk sapı + taş/çakmaktaşı baş ile balta, bıçak, sopa; alet dayanıklılığı ve kalitesi.
- **Referans:** SOTF, TLD (alet bozulması), üretim ağı.
- **Kod:** S1 — `EStickPrimitiveState::PrimitiveTool` (yalnız durum; item, hasar ve dayanıklılık yok).
- **Açık:** Çubuk durumu ile ürün item'ının ayrılması; tarif; dayanıklılık.
- **Görevler:** SOTF-M04, SOTF-K10, NET-R01.

#### M042 — Mızrak, yay ve ok yapmak
- **Tanım:** Mızrak (sertleştirilmiş sivri), yay (esnek, kuru, kirişli), ok (soyulmuş düz şaft + çakmaktaşı uç + tüy); mermi ve hasar durumu.
- **Referans:** SOTF, üretim ağı (bağırsak kirişi, tüy).
- **Kod:** S1 — `Spear`, `Bow`, `Arrow` durumları; tarif, mermi ve hasar yok.
- **Açık:** Ayrı item'lar, tarifler, fırlatma/mermi.
- **Görevler:** SOTF-M04, SOTF-K10.

#### M043 — Meşale ve basit ışık kaynakları yapmak
- **Tanım:** Çubuk + reçine/yağ + bez; yanma süresi ortak saatle; yakma, yeniden yakma, söndürme; yağmurda söner.
- **Referans:** SOTF, TLD (karanlık).
- **Kod:** S1 — `Torch` durumu; yanma süresi ve ışık yok.
- **Açık:** Yanma süresi, ışık, yakıt sistemi bağlantısı.
- **Görevler:** SOTF-K10, RISK-W04, RISK-H02.

#### M044 — Kazık, çit ve basit bariyer kurmak
- **Tanım:** Kazık zemine çakılır (kayada red); iki kazık arasına ray; sivri kazık hattı temas hasarı verir.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — `BarrierPart` durumu, `FPrimitiveAnchor`, `TryCommitGroundPlacement`.
- **Açık:** Zemin türü kuralı, ray, temas hasarı.
- **Görevler:** SOTF-K04, SOTF-K05.

#### M045 — İlkel tuzak ve tetik hattı kurmak
- **Tanım:** Tuzak eksik → kurulmuş → yerleştirilmiş → zemine bağlı → kurulu → tetiklendi → yeniden kur; tetik hacmi/çizgisi, hedef filtresi, hasar.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — `EPrimitiveTrapState`, `TryArmTrap`, `TryTriggerTrap`, `bTrapRequiresGrounding`; tetik hacmi ve hasar yok.
- **Açık:** Tetik algılama, hasar, 4 figürü/kement tarifleri.
- **Görevler:** SOTF-K09, 7D-R09.

#### M046 — Çubuktan depolama, raf ve mobilya yapmak
- **Tanım:** Çubuk tutucu, kurutma rafı, üçayak, raf, basit masa; her biri işlevsel bileşen taşır.
- **Referans:** SOTF, TLD (kurutma).
- **Kod:** S1 — `StoragePart` durumu; işlevsel yapı yok.
- **Açık:** İşlevsel aktörler.
- **Görevler:** SOTF-K08, SOTF-C03, NET-S10.

#### M047 — Basit bahçe ve hayvan destek yapıları kurmak
- **Tanım:** Bahçe çiti, sırık (fasulye), korkuluk, ağıl, kümes, yemlik; hayvan ve tarım sistemleriyle bağlantılı.
- **Referans:** FS, SOTF.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-K04, FS-A02, FS-A05, FS-G09.

#### M048 — Çubuk yapıları sökmek ve malzemeyi geri almak
- **Tanım:** Söküm durum ve dayanıklılığa göre malzeme verir; tek sahiplik işlemi.
- **Referans:** SOTF.
- **Kod:** S1 — `Salvaged` durumu; söküm işlemi yok.
- **Açık:** Söküm işlemi ve verim.
- **Görevler:** SOTF-D03.

---

## 5. Envanter ve üretim (M049–M058)

#### M049 — Dünya item'ını envantere almak
- **Tanım:** Fiziksel item envantere alınınca aktör kaybolur, kimlik ve sahiplik korunur; geri bırakılınca aynı kimlikle döner.
- **Referans:** Tümü.
- **Kod:** S2 — `UInventoryCraftingComponent::TryStorePhysicalActor`, `TryAddItem`, `TryDropOwnedItem`; sahiplik defteri.
- **Açık:** Taş ve çubuk aktörlerinin aynı yolla alınıp yeniden doğması.
- **Görevler:** ITEM-05.

#### M050 — Item istiflemek ve taşıma limitini uygulamak
- **Tanım:** İstif limiti ve toplam kütle limiti; bilinmeyen kütle sıfır sayılmaz; aşırı yük kademeleri.
- **Referans:** DS, TLD.
- **Kod:** S2 — `HasCapacityFor`, `GetCurrentWeightGrams`.
- **Açık:** Bilinmeyen kütle kuralı; elde taşınan kütükle tek kütle hesabı; aşırı yük kademeleri.
- **Görevler:** DS-C01, DS-C02, ITEM-04.

#### M051 — Item kategorisi, özellikleri ve hızlı erişim
- **Tanım:** Oyuncu item'ın kategorisini, kütlesini, dayanıklılığını, kondisyonunu görür; hızlı erişim yuvaları.
- **Referans:** TLD, Satisfactory.
- **Kod:** S1 — `EInventoryItemCategory`, `SetQuickAccessItem`; UI yok.
- **Açık:** UI, özellik alanları (dayanıklılık, kondisyon).
- **Görevler:** ITEM-01, RISK-A04.

#### M052 — Malzemeleri üretim alanına yerleştirmek
- **Tanım:** Oyuncu malzemeleri fiziksel bir üretim alanına/tezgâha koyar; uygun tarif algılanır.
- **Referans:** SOTF (crafting matı), Satisfactory (istasyon girişi).
- **Kod:** Yok — üretim doğrudan envanterden.
- **Açık:** Üretim alanı aktörü, giriş yuvaları.
- **Görevler:** ITEM-05, NET-L01.

#### M053 — Tarifi ve alternatif geçerli tarifleri çözmek
- **Tanım:** Aynı ürüne birden fazla yol; girdiye göre doğru tarif seçilir.
- **Referans:** Satisfactory, Hydroneer.
- **Kod:** S1 — `RegisterRecipe`, `FindRecipeIndex`; alternatif tarif yok.
- **Açık:** Tek tarif şeması, alternatif tarif.
- **Görevler:** NET-R01, NET-R05.

#### M054 — Eksik malzemeleri ve gereken miktarları göstermek
- **Tanım:** Hangi malzemeden kaç eksik olduğu ve hangi istasyonun gerektiği gösterilir.
- **Referans:** Satisfactory.
- **Kod:** S1 — `CanCraft` eksik denetimi; UI yok.
- **Açık:** Sebep kodu ve UI.
- **Görevler:** NET-R04, ITEM-05.

#### M055 — Üretimi onaylamak ve malzemeyi atomik tüketmek
- **Tanım:** Girdi tüketimi ve çıktı oluşumu tek işlem; hata durumunda tüm girdiler geri döner; tekrar çift çıktı üretmez.
- **Referans:** Satisfactory, Hydroneer.
- **Kod:** S2 — `UInventoryCraftingComponent::TryCraft`, crafting rollback fail-closed (#28).
- **Açık:** Yan ürün çıkışı; hata enjeksiyonu testi.
- **Görevler:** TXN-08, NET-R02, TEST-04.

#### M056 — Aletleri korumak ve kullanılmayan malzemeyi iade etmek
- **Tanım:** Alet tarifte tüketilmez ama dayanıklılık kaybeder; artan malzeme iade edilir.
- **Referans:** TLD, Satisfactory.
- **Kod:** S1 — `bIsTool` alanı; dayanıklılık kaybı ve artan iadesi tanımsız.
- **Açık:** Alet dayanıklılığı, artan iadesi.
- **Görevler:** NET-R01, RISK-B07 (alet bozulması ortak sistem).

#### M057 — Üretimi iptal etmek ve sonucu toplamak
- **Tanım:** Süreli üretim iptal edilince girdiler geri döner; biten üretim toplanır.
- **Referans:** Satisfactory.
- **Kod:** S1 — istasyonlarda `TryCollectCompletedJob`; elle üretimde iptal yok.
- **Açık:** İptal yolu, kısmi ilerleme kuralı.
- **Görevler:** NET-R04, ITEM-05.

#### M058 — Onarım kiti, koruma ekipmanı ve tuzak parçası üretmek
- **Tanım:** Onarım kiti (yapı/alet onarımı), giysi/koruma ve tuzak bileşenleri tariflerle üretilir.
- **Referans:** SOTF, TLD, 7D.
- **Kod:** Yok — tarif sistemi bunları tanımlamaya izin veriyor ama tanımlı tarif ve onarım işlevi yok (`InventoryCraftingComponent.cpp` içindeki "repair" sahiplik defteri onarımıdır).
- **Açık:** Tarifler ve onarım/koruma işlevleri.
- **Görevler:** 7D-S05, RISK-H08, SOTF-K09.

---

## 6. Ortak yapı ve hasar (M059–M070)

#### M059 — Tek otoriter yapısal graph kullanmak
- **Tanım:** Kütük, taş, çubuk, temel ve arazi desteği aynı graph'ta; aktör üzerindeki bağlantı bilgisi ikinci doğruluk kaynağı değildir.
- **Referans:** SOTF, 7D.
- **Kod:** S2 — `WorldStructuralGraphSubsystem` (`FStructuralNodeRecord`: Kind, ParentAnchorId, LocalLoad, MaxSupportedLoad, bGroundSupported, Damage, DamageThreshold, Revision); kütük entegrasyonu (#13).
- **Açık:** Aktörlerde `ParentAnchorId`/`AttachedPieces` kopyası; malzemeler arası bağlantı.
- **Görevler:** STRUCT-01, SOTF-M02.

#### M060 — Zeminden yüke destek yolunu doğrulamak
- **Tanım:** Her parçanın zemine/temele/arazi ankrajına kesintisiz destek yolu vardır; zemin desteği gerçek zemin denetimiyle verilir.
- **Referans:** SOTF, 7D.
- **Kod:** S1 — `HasValidSupportPath`, `SetGroundSupported` (bayrakla veriliyor).
- **Açık:** Gerçek zemin/temel denetimi, arazi revizyonu bağlantısı.
- **Görevler:** STRUCT-03, 7D-V15.

#### M061 — Açıklık, konsol ve yük limitlerini uygulamak
- **Tanım:** Malzeme profiline göre taşıma kapasitesi, kiriş açıklığı ve konsol uzanma limitleri.
- **Referans:** 7D, SOTF.
- **Kod:** S1 — `ComputeAggregateLoad`, `MaxSupportedLoad`; açıklık/konsol yok.
- **Açık:** Açıklık ve konsol, çoklu destekte yük paylaşımı.
- **Görevler:** 7D-S01, 7D-S02, SOTF-B04.

#### M062 — Yapısal bağlantılara hasar eşiği uygulamak
- **Tanım:** Hasar eşiği aşılan parça bağlantısını kaybeder; hasar tek yoldan gelir.
- **Referans:** 7D.
- **Kod:** S2 — `ApplyDamage` + `DamageThreshold`; baskın adaptörü bunu kullanıyor.
- **Açık:** Hasar görseli, hasar kaynağı kimliği.
- **Görevler:** 7D-R05.

#### M063 — Bağımlı parçaları kararlı sırada çökertmek
- **Tanım:** Aynı yapı her seferinde aynı sırada çöker; aşırı yük yeniden dağıtımı adım adım.
- **Referans:** 7D.
- **Kod:** S1 — `CollectDependentsDepthFirst` (yapraktan köke plan).
- **Açık:** Yük yeniden dağıtımı, fiziksel çökme.
- **Görevler:** 7D-S03, SOTF-D02.

#### M064 — Kapı, pencere ve duvar açıklığı oluşturmak ve kaldırmak
- **Tanım:** Duvara vurularak açıklık; komşu parçalar segmentlere bölünür; kapı aktörü takılır, açılır, kilitlenir, kırılır.
- **Referans:** SOTF.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-L09, SOTF-B07.

#### M065 — Zemin, çatı, rampa ve merdiven yapmak
- **Tanım:** Kirişlere oturan zemin; çatı ve barınak bölgesi; merdiven navmesh ile çalışır.
- **Referans:** SOTF.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-B04, SOTF-B05, SOTF-B06, SOTF-K07.

#### M066 — Köprü, ip geçit ve yükseltilmiş yol yapmak
- **Tanım:** İki kıyıda destekli köprü; ip köprü; rota grafiğine kalıcı kenar; sel yüksekliğine göre dayanım.
- **Referans:** DS, SOTF.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** DS-X02, DS-X03, DS-X04, DS-X05.

#### M067 — Kapı (avlu), çit, korkuluk ve savunma duvarı yapmak
- **Tanım:** Avlu kapısı, çit, korkuluk, sivri kütük savunma duvarı; AI için yol maliyeti ve kırılabilir hedef.
- **Referans:** SOTF, 7D.
- **Kod:** Yok — çubukta `BarrierPart` durumu dışında karşılık yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-K05, SOTF-L06, SOTF-B07, 7D-R04.

#### M068 — Durumu kopyalamadan yapıyı hasarlandırmak ve onarmak
- **Tanım:** Hasar ve onarım graph düğümünde tek kayıt; aktör ve UI bu kaydı okur.
- **Referans:** 7D.
- **Kod:** S1 — `ApplyDamage` var, onarım yok.
- **Açık:** Onarım yolu, görsel kademe.
- **Görevler:** 7D-S05.

#### M069 — Yapıları sökmek ve kaynakları geri almak
- **Tanım:** Söküm bağımlıları denetler, parçayı durumuna göre item olarak verir.
- **Referans:** SOTF, Satisfactory.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** SOTF-D03.

#### M070 — Destekleyen parçanın güvensiz alınmasını/kaldırılmasını engellemek
- **Tanım:** Üzerinde yük olan parça alınamaz veya alınmadan önce bağımlı çökme önizlenir.
- **Referans:** SOTF.
- **Kod:** S2 — `CanSafelyRemove`; bağımlısı olan kütüğün depolanması reddediliyor.
- **Açık:** Çökme önizlemesi UI.
- **Görevler:** SOTF-D03.

---

## 7. Arazi, dünya durumu ve kazı (M071–M080)

#### M071 — Dünya durumunu kalıcı chunk'lara bölmek
- **Tanım:** Arazi ve dünya durumu chunk'larda tutulur; yalnız değişen chunk kaydedilir.
- **Referans:** 7D (smooth voxel).
- **Kod:** S1 — `WorldTerrainStateSubsystem` hücre kaydı (`FWorldTerrainCellState`); chunk ve yoğunluk modeli yok.
- **Açık:** Chunk, yoğunluk (SDF), mimari kararı.
- **Görevler:** 7D-V01, 7D-V03, 7D-V10.

#### M072 — Yalnız yapılandırılmış kazılabilir katmanları düzenlemek
- **Tanım:** İzinli derinlik katmanları ve korunan bölgeler dışında kazı reddedilir.
- **Referans:** 7D, Hydroneer.
- **Kod:** S1 — `PermittedDepthLayers`, `MaxEditableDepthLayers`, `MaxDepthPerCell`, `TryApplyCellEdit`.
- **Açık:** Korunan bölgeler, malzeme tabakası tablosu.
- **Görevler:** TERR-11, TERR-13, 7D-T01 (v1) / 7D-V07.

#### M073 — Geçerli geometri ve collision ile çukur kazmak
- **Tanım:** Kazı görünür yüzeyi ve collision'ı aynı commit'te değiştirir; karakter çukura iner.
- **Referans:** 7D, Hydroneer.
- **Kod:** Yok — render/collision adaptörü yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-V04, 7D-V05, 7D-V07.

#### M074 — Yatay tünel ve küçük oda kazmak
- **Tanım:** Yer altına yatay tünel ve oda; navmesh güncellenir.
- **Referans:** 7D.
- **Kod:** Yok.
- **Açık:** Tamamı (heightfield ile mümkün değil; voxel gerekli).
- **Görevler:** 7D-V03..V07.

#### M075 — Desteksiz yer altı çökmesini önlemek
- **Tanım:** Tavan açıklığı malzeme limitini aşar ve destek yoksa çöker; kütük direk/taş ayak destek verir.
- **Referans:** 7D.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-V12, SOTF-D01.

#### M076 — Arazi değişikliklerini kayıt/yüklemede korumak
- **Tanım:** Seed'den temel arazi + değişen chunk delta'ları; aynı kazı tekrar oynatılmaz.
- **Referans:** 7D, DS.
- **Kod:** S1 — hücre snapshot'ı kayda giriyor; `Receipts` `UPROPERTY(Transient)` ve `RestoreSnapshot` içinde `Receipts.Reset()`.
- **Açık:** Kalıcı makbuz, chunk delta, yükleme sırası.
- **Görevler:** TERR-12, 7D-V10.

#### M077 — Su seviyesi, akış ve yerel sel simülasyonu
- **Tanım:** Nehir seviyesi yağışla gecikmeli yükselir; seviye kıyıyı aşınca sel yayılır; hendek ve set suyu yönlendirir.
- **Referans:** DS (yaşayan dünya).
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** DS-W01..W07.

#### M078 — Toprak nemi, besin ve yüzey akışı
- **Tanım:** Tarla hücresinde nem, azot, pH, organik madde; yağmur ve sulama; yamaçta yüzey akışı.
- **Referans:** FS.
- **Kod:** S1 — yalnız ürün kaydında `Moisture`/`MinimumGrowthMoisture`; hücre modeli ve besin yok.
- **Açık:** Hücre katmanları, yüzey akışı.
- **Görevler:** FS-F01, FS-F02, FS-F05, FS-F06.

#### M079 — Yangın yayılması, çürüme ve malzeme maruziyeti
- **Tanım:** Ateş kuru malzemeye yayılır, yağmur söndürür; ıslak/toprakla temas eden ahşap çürür; yapılar havaya maruz kalarak aşınır.
- **Referans:** DS (aşınma), TLD, SOTF.
- **Kod:** Yok.
- **Açık:** Tamamı. **Bağlı görev belgesinde yangın yayılması için görev eksik; eklenmeli.**
- **Görevler:** DS-X09, SOTF-L12, SOTF-S08; yeni: **FIRE-01** (yangın yayılması: malzeme yanıcılığı × nem × rüzgâr; yağmur söndürür; yanan parça yanık durumuna geçer).

#### M080 — Çevresel yıkıntı ve ipuçları yerleştirmek ve keşfetmek
- **Tanım:** Az sayıda not ve çevresel sahne; her biri bir yere bağlı ve pratik bilgi taşır; anlatı zorunlu değildir.
- **Referans:** Subnautica.
- **Kod:** Yok — yalnız `RecordDiscovery(FName, FVector, double)` kaydı (M119).
- **Açık:** Keşif noktası aktörü, sahneler, içerik.
- **Görevler:** SN-N01, SN-N03, SN-N04.

---

## 8. Tarım ve hayvancılık (M081–M090)

#### M081 — Dünya zamanını deterministik simülasyon adımlarıyla ilerletmek
- **Tanım:** Tarım ve hayvanlar ortak sabit adımla ilerler; aynı adım iki kez işlenmez.
- **Referans:** FS.
- **Kod:** S2 — `UWorldAgricultureSubsystem::CanAdvanceAgricultureStep/AdvanceAgricultureStep`, koordinatör.
- **Açık:** Mevsim takvimi.
- **Görevler:** FS-G02, CLOCK-03.

#### M082 — Tohum ekmek ve toprak/mevsim koşullarını doğrulamak
- **Tanım:** Ekim tohum item'ı tüketir; sürülmemiş hücre, yanlış mevsim veya dolu hücre reddedilir.
- **Referans:** FS.
- **Kod:** S1 — `RegisterCrop` (doğrudan kayıt; tohum, toprak ve mevsim denetimi yok).
- **Açık:** Parsel/hücre, tohum tüketimi, mevsim.
- **Görevler:** FS-F01, FS-F03, FS-F04.

#### M083 — Ürünü tanımlı evrelerden geçirerek büyütmek
- **Tanım:** Evre sayısı ve süreleri ürün tanımından; yalnız izinli aylarda büyüme.
- **Referans:** FS.
- **Kod:** S1 — sabit `ECropGrowthStage {Seed, Sprout, Growing, Mature, Withered}`.
- **Açık:** Ürün data asset'i, takvim.
- **Görevler:** FS-G01, FS-G02.

#### M084 — Sulama, besin ve bitki stresi uygulamak
- **Tanım:** Nem, azot, pH ve ot rekabeti büyüme ve sağlığı etkiler; kuraklık stresi.
- **Referans:** FS.
- **Kod:** S1 — `SetCropMoisture`, `MinimumGrowthMoisture`.
- **Açık:** Besin, pH, ot, su item'ı ile sulama.
- **Görevler:** FS-F05..F08.

#### M085 — Ürün hasat etmek ve verim hesaplamak
- **Tanım:** Hasat ana ve yan ürünü item olarak üretir; verim çarpanlara ayrılmış formülle hesaplanır; tekrarda çoğalmaz.
- **Referans:** FS.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** FS-G03, FS-G04, FS-G05.

#### M086 — Çiftlik hayvanı beslemek ve bakmak
- **Tanım:** Hayvan yemlik/suluktan item tüketir; tür bazlı yem ve barınak ihtiyacı.
- **Referans:** FS.
- **Kod:** S1 — `FeedAnimal(AnimalId, HungerReduction, ExpectedRevision)`; item tüketmiyor.
- **Açık:** Tür tanımı, yemlik/suluk aktörü, item tüketimi.
- **Görevler:** FS-A01, FS-A02.

#### M087 — Hayvan büyümesi ve sağlığını ilerletmek
- **Tanım:** Yavrudan yetişkine büyüme; bakım, temizlik ve soğuk sağlığı etkiler; hastalık bulaşır.
- **Referans:** FS.
- **Kod:** S1 — `FWorldAnimalState` (`Health`, `Hunger`, `HungerPerHour`).
- **Açık:** Büyüme evresi, temizlik, hastalık.
- **Görevler:** FS-A06, FS-A07.

#### M088 — Süt, yumurta, yün veya diğer ürünleri üretmek
- **Tanım:** Ürün sahiplik defterinde doğan item'dır; toplanmazsa bozulur veya verim düşer.
- **Referans:** FS.
- **Kod:** S1 — `ProductUnits` sayacı.
- **Açık:** Item olarak doğma, toplama eylemi.
- **Görevler:** FS-A03.

#### M089 — Hayvanları barınak kapasitesi içinde üretmek
- **Tanım:** Koşullar sağlanınca gebelik ve yavru; kapasite doluysa üreme durur.
- **Referans:** FS.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** FS-A08.

#### M090 — Yem, gübre ve toprak besin döngülerini bağlamak
- **Tanım:** Hasat yeme, hayvan gübreye, gübre tarlaya döner.
- **Referans:** FS, üretim ağı.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** FS-A12, FS-F06, FS-F11.

---

## 9. Baskın ve tehdit AI (M091–M100)

#### M091 — Tehdit olaylarını zamanlamak ve geçitlemek
- **Tanım:** Baskın takvimi ortak saate bağlı; uyarı belirtileri; aynı adım ikinci baskın açmaz.
- **Referans:** 7D.
- **Kod:** S2 — `WorldThreatDirectorSubsystem::InitializeDirector`, `AdvanceDirectorAtTime`, `GetNextRaidTimeSeconds`.
- **Açık:** Uyarı belirtileri, gün sayısına bağlı artış.
- **Görevler:** 7D-R01.

#### M092 — Zorluğu oyuncu ve dünya bağlamından değerlendirmek
- **Tanım:** Zorluk gün sayısı, üs değeri, savunma ve faaliyet ısısından hesaplanır.
- **Referans:** 7D.
- **Kod:** S1 — baskın kaydında `Pressure` alanı; girdi hesaplaması yok.
- **Açık:** Girdi formülü, veri tablosu.
- **Görevler:** 7D-R11, 7D-R03.

#### M093 — Düşman gruplarını, dalgaları ve kaynak bütçesini planlamak
- **Tanım:** Dalga sayısı, dalga başı bütçe, tür karışımı.
- **Referans:** 7D.
- **Kod:** Yok — tek baskın kaydı, dalga planı yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-R01, 7D-R08, 7D-R11.

#### M094 — Doğuş kapasitesi ve kuyruk limitlerini uygulamak
- **Tanım:** Aynı anda aktif düşman sınırı; kuyruğa alınan doğuşlar.
- **Referans:** 7D.
- **Kod:** Yok — adaptör tek Pawn tutuyor.
- **Açık:** Tamamı.
- **Görevler:** 7D-R01.

#### M095 — Geçerli doğuş konumu seçmek
- **Tanım:** Navigasyonlu zemin, oyuncudan mesafe, görüş dışı, arazi engelleri; başarısız doğuş dalgayı tüketmez.
- **Referans:** 7D.
- **Kod:** S1 — `WorldRaidAdapterSubsystem` sabit doğuş konumunda `SpawnActor<APawn>`.
- **Açık:** Doğuş bölgesi çözümleyici.
- **Görevler:** 7D-R07.

#### M096 — Hedef seçmek ve navigasyon yolu hesaplamak
- **Tanım:** Hedef (oyuncu, depo, üretim, savunma) tanımlı girdilerle seçilir; yol maliyeti engel sağlığını içerir.
- **Referans:** 7D, DS (ortak rota grafiği).
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-R04, DS-P01.

#### M097 — Saldırı, atlatma ve yapı hasarını çözmek
- **Tanım:** AI yolu kapatan parçaya saldırır veya kazarak altından geçer; parça düşünce yol yeniden hesaplanır.
- **Referans:** 7D.
- **Kod:** S1 — adaptör graph düğümlerine periyodik `ApplyDamage` uyguluyor; erişim, hedefleme ve kazma yok.
- **Açık:** Erişim tabanlı saldırı, kazıcı düşman.
- **Görevler:** 7D-R05, 7D-R06.

#### M098 — Dalga durum makinesini ilerletmek
- **Tanım:** Planlandı → uyarı → aktif → püskürtüldü/başarısız/iptal; her geçiş idempotent.
- **Referans:** 7D.
- **Kod:** S2 — `EThreatRaidState`, `ResolveCurrentRaid`, `IsTerminal`, snapshot/restore.
- **Açık:** Uyarı aşaması ve dalga bazlı durumlar.
- **Görevler:** 7D-R01.

#### M099 — Başarısız baskını iptal etmek, kesmek ve kurtarmak
- **Tanım:** İptal veya kayıt/yükleme kesintisinde baskın tutarlı kapanır; ikinci dalga veya hasar üretmez.
- **Referans:** 7D.
- **Kod:** S1 — `CancelCurrentRaid`.
- **Açık:** Kayıt sırasında aktif baskın kuralı, doğmuş Pawn temizliği.
- **Görevler:** 7D-R01, SAVE-09.

#### M100 — Yerleşim gürültüsü, stok ve savunmaya göre baskıyı yeniden değerlendirmek
- **Tanım:** Faaliyet ısısı ızgarası, depolanan değer ve savunma gücü baskı puanını değiştirir; eşikte gözcü gelir.
- **Referans:** 7D.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** 7D-R03, 7D-R11.

---

## 10. Lojistik, üretim, survival ve keşif (M101–M119)

#### M101 — Araba, kızak ve depolama raflarını yüklemek ve boşaltmak
- **Tanım:** Kızak/araba fiziksel olarak itilir; yuvaları malzemeye göre; devrilince yük dökülür, her item tek sahipli.
- **Referans:** SOTF, DS.
- **Kod:** S1 — genel depo rezervasyonu `WorldStorageSubsystem::TryReserveIncoming/CommitIncomingReservation`; kızak/araba aktörü yok.
- **Açık:** Kızak ve araba aktörleri.
- **Görevler:** SOTF-C02, SOTF-C03.

#### M102 — Malzemeyi ip hattı veya zipline ile taşımak
- **Tanım:** İki direk arası hat; kargo `InTransit`; direk yıkılınca yoldaki kargo düşer.
- **Referans:** DS, Satisfactory.
- **Kod:** S1 — `WorldCargoTransportSubsystem::StartTransport/AdvanceCargoStep/TryDeliver` (mantıksal; fiziksel hareket yok).
- **Açık:** Fiziksel taşıyıcı ve hat aktörü.
- **Görevler:** DS-X08, NET-L02.

#### M103 — İlkel bıçkı ve üretim zincirleri kurmak
- **Tanım:** Kütük → plank → ürün zinciri istasyonlarda; yan ürünler başka zincire girer.
- **Referans:** Satisfactory, Hydroneer.
- **Kod:** S2 — `WorldProductionSubsystem::TryIngestWorldOwnedLogPiece`, `TryMaterializeStockAsWorldLog`, istasyon/tarif/stok (yalnız kütük için).
- **Açık:** Kütük dışı malzemeler, yan ürün, ağ doğrulayıcı.
- **Görevler:** NET-R01..R03, NET-S01..S11.

#### M104 — Üretimi kuyruğa almak ve engel sebeplerini göstermek
- **Tanım:** İstasyon durumları (boşta, çalışıyor, girdi bekliyor, çıktı dolu, yakıt yok) sebep koduyla.
- **Referans:** Satisfactory.
- **Kod:** S1 — `TryEnqueueJob`, `AdvanceProductionStep`, engel durumları; UI yok.
- **Açık:** Tam durum makinesi, UI.
- **Görevler:** NET-R04.

#### M105 — Yakıt, ısı ve pişirme istasyonlarını yönetmek
- **Tanım:** Tek ısı kaynağı bileşeni hem ısıtır hem pişirir; yakıt enerji değeriyle tükenir.
- **Referans:** Frostpunk, TLD.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** RISK-H02, RISK-F01, NET-S01.

#### M106 — Yol ve köprü ile rota maliyetini düşürmek
- **Tanım:** Yol, köprü, basamak taşı ve iz rota maliyetini kalıcı düşürür; maliyet yüzey, eğim, su ve havaya bağlıdır.
- **Referans:** DS.
- **Kod:** S1 — `WorldLogisticsRouteSubsystem` (Dijkstra); `FLogisticsRouteEdge{FromNode, ToNode, Cost, bEnabled}` sabit maliyetli.
- **Açık:** `RouteCostProvider`, gerçek aktör uç noktaları, ortak grafik.
- **Görevler:** DS-P01, DS-P02, DS-X02, DS-X06.

#### M107 — Barınak, sıcaklık ve günlük hayatta kalma seçimlerini yönetmek
- **Tanım:** Oyuncunun günlük kararları (kesim, av, onarım, uyku) ölçülebilir sonuç üretir; yerel kaynaklar tükenir.
- **Referans:** TLD, Frostpunk.
- **Kod:** S1 — `ShelterProtection` sayısı.
- **Açık:** Oda algılama, yerel tükenme, bütçe görünümü.
- **Görevler:** RISK-H07, RISK-F02, RISK-F03, SOTF-B05.

#### M108 — Kaynak, dayanıklılık ve depo durumunu oyuncuya göstermek
- **Tanım:** Stok, kalan yakıt saati, alet dayanıklılığı, depo doluluğu yetkili durumdan okunur.
- **Referans:** Satisfactory, TLD.
- **Kod:** Yok — UI yok.
- **Açık:** Tamamı.
- **Görevler:** RISK-F03, RISK-A04, NET-R04, ITEM-08.

#### M109 — Dünya ve üretim genelinde entegre kayıt/yüklemeyi göstermek
- **Tanım:** Kayıt → dünyayı değiştir → yükle sonrası aktör, graph, sahiplik, arazi, stok, ürün, hayvan, kargo, baskın, survival ve keşif aynı.
- **Referans:** Tümü.
- **Kod:** S2 — `WorldSaveCoordinatorSubsystem` tüm alt sistemlerin snapshot'larını topluyor; çalışma zamanı kanıtı yok.
- **Açık:** Manifest/hash karşılaştırması, aktör yeniden yaratma.
- **Görevler:** SAVE-11, SAVE-12.

#### M110 — Hava ve mevsim geçişlerini simüle etmek
- **Tanım:** Sıcaklık, rüzgâr (hız+yön), yağış miktarı, bulut, sis; mevsim takvimi; deterministik seed.
- **Referans:** DS, TLD, FS.
- **Kod:** S1 — `EWorldWeatherState {Clear, Rain, Wind, Snow, Storm, Heatwave}` + `Severity`, `SetWeather`; mevsim yok.
- **Açık:** Sürekli değerler, mevsim, geçiş kuralları, tahmin.
- **Görevler:** RISK-W01, RISK-W02, FS-G02, DS-W04.

#### M111 — Soğuk, sıcak ve ıslaklık maruziyeti uygulamak
- **Tanım:** Hissedilen sıcaklık fonksiyonu; ıslaklık giysi ve ısıyı etkiler.
- **Referans:** TLD.
- **Kod:** S1 — `ColdDamagePerHour`, `Wetness`.
- **Açık:** Tek sıcaklık fonksiyonu, rüzgâr soğutması, ıslanma-kuruma döngüsü.
- **Görevler:** RISK-H01, RISK-H06.

#### M112 — Açlık ve susuzluk tüketimini izlemek
- **Tanım:** Kalori ve su depoları; faaliyet maliyeti; yiyecek/içecek item tüketimiyle dolum.
- **Referans:** TLD.
- **Kod:** S1 — `HungerPerHour`, `ThirstPerHour`; item tüketimi yok.
- **Açık:** Kalori modeli, item etkileri.
- **Görevler:** RISK-B01, RISK-B02.

#### M113 — Dinlenme ve faaliyet sınırlarıyla stamina toparlamak
- **Tanım:** Stamina koşma, taşıma, kesim ile düşer; dinlenmeyle toparlanır; yorgunluk maksimumu düşürür.
- **Referans:** TLD, SOTF.
- **Kod:** S1 — `Stamina` alanı; faaliyet maliyeti yok.
- **Açık:** Aktivite maliyet tablosu.
- **Görevler:** RISK-B01.

#### M114 — Uyku ve yorgunluk etkilerini uygulamak
- **Tanım:** Uyku saati ilerletir (tek kez), yorgunluğu giderir; soğukta uyku tehlikeli; uyandırma koşulları.
- **Referans:** TLD.
- **Kod:** S1 — `Fatigue`, `FatiguePerHour`; uyku eylemi yok.
- **Açık:** Uyku eylemi, zaman atlama.
- **Görevler:** RISK-B04.

#### M115 — Yaralanma, hastalık ve zehirlenme durumlarını uygulamak
- **Tanım:** Data-driven rahatsızlıklar: tetik, etki, süre, tedavi, kalıcı iz.
- **Referans:** TLD, 7D.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** RISK-B05, RISK-B06, RISK-B07.

#### M116 — Yiyecek kalitesini ve zamanla bozulmasını korumak
- **Tanım:** Kondisyon yüzdesi; sıcaklık ve kap türü bozulmayı etkiler; kurutma, tütsüleme, tuzlama ömrü uzatır.
- **Referans:** TLD, FS.
- **Kod:** Yok.
- **Açık:** Tamamı.
- **Görevler:** RISK-F04, FS-P01, NET-S10.

#### M117 — Giysi yalıtımı ve koruyucu ekipman uygulamak
- **Tanım:** 10 yuva; sıcaklık, rüzgâr geçirmezlik, ıslanma, kütle, dayanıklılık; değer giyilen item'dan türetilir.
- **Referans:** TLD.
- **Kod:** S1 — `ClothingInsulation` serbest sayı (`UpdateProtection`).
- **Açık:** Giysi yuvaları ve item özellikleri.
- **Görevler:** RISK-H05, RISK-H08.

#### M118 — Barınak kalitesinin çevresel maruziyeti etkilemesi
- **Tanım:** Çatı, duvar sızdırmazlığı (sıva), açıklık ve ısı kaynağı iç sıcaklığı belirler.
- **Referans:** TLD, Frostpunk, SOTF.
- **Kod:** S1 — `ShelterProtection` serbest sayı.
- **Açık:** Oda algılama ve bütünlük puanı.
- **Görevler:** RISK-H07, SOTF-B05, SOTF-K06.

#### M119 — Keşiflerin kalıcı olması ve dünya bağlamını açması
- **Tanım:** Keşif kaydı kalıcıdır, ikinci kez ödül vermez; defterde görünür; pratik bilgi sağlar; hiçbir sistem nota kilitli değildir.
- **Referans:** Subnautica.
- **Kod:** S1 — `UWorldSurvivalSubsystem::RecordDiscovery`, `GetDiscoveries`, snapshot.
- **Açık:** Keşif noktası aktörü, defter, pratik bilgi.
- **Görevler:** SN-N01, SN-N02, SN-N06.

---

## 11. 119 listesinde olmayan, yeni görev belgesinden gelen mekanik adayları

Kayıt kuralı (REG-01) gereği mevcut ID'ler değiştirilmez. Aşağıdakiler **yeni ID önerisidir**; kabul edilirse kayda M120 ve sonrası olarak eklenir.

| Öneri | Mekanik | Kaynak | Not |
|---|---|---|---|
| M120 | Smooth voxel yoğunluk arazisi ve yüzey çıkarma | 7D-V03..V05 | M071–M075'in teknik temeli |
| M121 | Gevşek malzemede toprak kayması | 7D-V11 | DS yaşayan dünya |
| M122 | Kazıcı düşmanın araziyi kazması | 7D-R06 | Baskın |
| M123 | Yağışa bağlı nehir seviyesi ve geçilebilirlik sınıfları | DS-W01..W04 | M077'den ayrı: geçiş kuralı |
| M124 | Donma ve buz kalınlığı | DS-W06 | Kış riski |
| M125 | Basamak taşı, sal ve feribot halatı | DS-X01, DS-X05 | Nehir çözümleri |
| M126 | Kullanımla oluşan iz ve yol kademesi | DS-X06 | Kalıcı değişim |
| M127 | Dünya değişim günlüğü | DS-P03 | Kalıcı olay kaydı |
| M128 | Malzemeler arası anchor uyumluluğu | SOTF-M02 | M038'in temeli |
| M129 | Mera ve otlatma | FS-A05 | Hayvancılık |
| M130 | Kompost ve ekim nöbeti | FS-F10, FS-F11 | Toprak |
| M131 | Hayvan kesimi ve karkas işleme | FS-A09 | Üretim ağı |
| M132 | Yırtıcı baskısı | FS-A10 | Hayvancılık + AI |
| M133 | Yerel kaynak tükenmesi ve yavaş yenilenme | RISK-F02 | Risk zorunluluğu |
| M134 | Hava tahmini ve belirtiler | RISK-W02 | Risk zorunluluğu |
| M135 | Ateş yakma başarı olasılığı ve ateşin yaşamı | RISK-H03, RISK-H04 | M105'ten ayrı |
| M136 | Atmosferik beden geri bildirimi | RISK-A01..A03 | Risk hissi |
| M137 | Yangın yayılması | FIRE-01 | M079'un parçası olarak da tutulabilir |
| M138 | Üretim ağı yan ürünleri ve ağ doğrulayıcı | NET-R02, NET-R03 | Üretim ağı |
| M139 | Kömür ve kireç ocağı | NET-S02, NET-S03 | Üretim ağı |
| M140 | Suda ayıklama ve oluk düzeneği | NET-D02, NET-D03 | Hydroneer |
| M141 | Eritme, körük ve dövme | NET-S04, NET-S05 | Metal |
| M142 | Tabakhane ve lif işleme | NET-S06, NET-S07 | Hayvan ve bitki ağı |
| M143 | Su kanalı ağı | NET-W01 | Su |
| M144 | Notlarla isteğe bağlı araştırma zinciri | SN-N05 | Anlatı |

---

## 12. Önerilen ilk kapanış sırası

Önce derleme sağlığı (REL-02: `HarvestableTreeActor.h:100`), sonra her dikey dilimin kapsadığı mekanikler:

| Dilim | Kapanacak mekanikler |
|---|---|
| A — Orman, malzeme, ilk barınak | M001–M004, M009–M013, M015–M018, M024–M026, M029, M031–M032, M038–M040, M044, M049–M050, M059–M060, M070, M105, M113 |
| B — Yaşayan nehir ve köprü | M066, M077, M106, M110, M123, M125 |
| C — Smooth voxel kazı ve kil/kireç | M071–M076, M103, M120, M139 |
| D — Tarla ve hayvan | M078, M081–M088, M090 |
| E — Kış ve risk | M008, M107, M111–M118, M133–M136 |
| F — Baskın gecesi | M023, M045, M061–M063, M067, M091–M100, M122 |
| G — Notlar ve keşif | M080, M119, M144 |
| H — Demir ve üretim ağı | M053–M058, M101–M104, M108, M138, M140–M143 |
| Son — entegre kanıt | M005–M007, M109 |

Her mekanik ancak kendi **Tanım** satırındaki pozitif ve negatif sonuç UE 5.8 içinde gösterildiğinde kayıtta `RuntimeTest = pass` olarak işaretlenir.
