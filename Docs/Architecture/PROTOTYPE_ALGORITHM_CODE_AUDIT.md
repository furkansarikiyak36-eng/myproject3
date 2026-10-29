# MyProject3 — Matematiksel ve Algoritmik Kod Denetimi

**Dal:** fix/vertical-slice-b-core-loop  
**Kapsam:** Üretim C++ kaynakları, ilişkili sistem mimarisi ve dikey kesit oluşturma betikleri.  
**Hariç:** Derleme, otomatik test çalıştırma, Unreal Editor/PIE ve runtime deneyi. Bu raporda bunlar kanıt olarak kullanılmadı.

## 1. Denetimin amacı

Bu rapor yalnızca kodun var olup olmadığını değil; algoritmaların matematiksel olarak tutarlı olup olmadığını, varsayımların açık olup olmadığını, farklı sistemlerin aynı kuralları kullanıp kullanmadığını ve bir prototipin henüz hangi anlamda yaklaşık kaldığını inceler. Bu statik inceleme runtime doğrulaması değildir. M001–M119 kayıtlarının durumları bu raporla değiştirilmez.

## 1A. Güncel uygulama durumu — 29 Ekim 2026

Aşağıdaki A01–A39 bulguları ilk statik inceleme sırasında kaydedilen risklerdir. PR #51'de sonraki kod değişiklikleriyle bazıları düzeltildi, bazıları kısmen kapatıldı ve iki bulgu kaynak kodu yeniden kontrol edildiğinde yanlış pozitif çıktı. Bu durum güncellemesi **kaynak kodundaki değişiklikleri** kaydeder; Unreal build/Editor/PIE veya test sonucu değildir.

| Bulgu | Güncel durum |
|---|---|
| A01 voxel epsilon | **Kodda düzeltildi:** küçük negatif density artık `-epsilon`, pozitif/sıfır değer `+epsilon` alıyor; işaret sınıfı korunuyor. |
| A02 VerticalSlice_B driver | **Kısmen düzeltildi:** `CreateVerticalSliceBMap.py` açık `WorldDemoBootstrapActor` ekliyor. `RebuildVerticalSliceBFromScratch.py` hâlâ yalnız sunum amaçlı ground+lighting sahnesi üretir; onu oyun dünyasının yerine kullanmamak gerekir. |
| A03 save/load preflight | **Kısmen güçlendirildi:** clock/threat, ownership, inventory, üretim reçeteleri/stok rezervleri, terrain receipt'leri, survival/discovery verileri, kargo/depo kapasitesi ve yapısal kimlik referansları mutasyon öncesi doğrulanıyor; capture sırasında canlı alt sistemleri değiştiren doğrulama probe'ları kaldırıldı. Aday yükleme başarısızsa eski snapshot telafi olarak yeniden uygulanıyor ve telafi de başarısız olursa coordinator faulted oluyor. Bu yine çok-domain atomik commit garantisi değildir. |
| A04 dinamik fiziksel aktör manifesti | **Kod eklendi, statik kapsamda:** eksik identity'li aktörler saved GUID ile deferred spawn ediliyor, fazla aktörler kaldırılıyor; actor/component state, component-local transform ve fizik durumu geri yükleniyor. Restore sırasında log/stone/stick bağlantı temizliği, log sahiplik tüketimi ve depo içerik transferi yan etkileri bastırılıyor; depo listeleri ownership ledger'dan yeniden kuruluyor. Schema v12 öncesi manifesti olmayan kayıtlar reddediliyor; v16→v17 migration eski kayıtlarda child transform'larını sınıf varsayılanından doldurur, eski özel offset/scale değerleri kurtarılamaz. Blueprint/runtime component layout ve lifecycle motor içinde doğrulanmadı. |
| A06 baskın fixed-step entegrasyonu | **Kod eklendi, statik kapsamda:** coordinator adaptörü logical tick commit'inden önce preflight ediyor ve sonrasında çalıştırıyor; post-commit hata fail-closed duruma alınıyor. Çoklu hedef hasarı graph copy üzerinde doğrulanıp tek map-swap ile uygulanıyor; collapse öncesi hedef/aktör/bağlantı ve revision sınırları kontrol ediliyor. Pulse watermark ve raid başına sabit saldırı aralığı kaydediliyor; işlem ledger kilidi kalıcı watermark commit edildikten sonra budanıyor. Aktör yıkımı Unreal seviyesinde geri alınabilir değil; tam AI/navmesh ve gerçek yerleşim hedef çözümleyicisi de hâlâ yok. |
| A05 destek/kaldırma | **Kodda güçlendirildi:** secondary-support kaldırımı son aggregate yükleri doğruluyor; node kaldırma/reparent sonrası grafiği geçici kopya üzerinde doğrulayıp başarısızsa geri alıyor. Eşit yük paylaşımı yine bir prototip sezgiselidir. |
| A07–A08 kütük kesim reçeteleri | **Kodda güçlendirildi:** çıktı uzunluklarına göre beklenen merkez aralığı doğrulanıyor; aynı uzunluk/şekil anahtarına ait yinelenen mesh eşleşmeleri reddediliyor. |
| A09 fiziksel kesim geometrisi | **Kısmen kapatıldı:** ölçeklemeyle oluşan uzunluk artık `bHasCustomLengthScale` ile işaretleniyor; bu parçaların canonical kesim/definition ve inventory akışı tarafından sessizce normalleştirilmesi reddediliyor. Mesh'i gerçek bir düzlemle kesen geometri üretimi hâlâ yok. |
| A10 ağaç verimi | **Kodda düzeltildi:** verim bandı için dik duran yükseklik önceden saklanıyor; devrilme sonrası dünya Z sınırları verim sınıfını değiştirmiyor. |
| A11–A13 rota/fiziksel hasar | **Kodda güçlendirildi:** eşit maliyetli rota seçimi deterministik; sonlu olmayan hasar/impulse engelleniyor; aynı darbede component ve generic damage API'si birlikte çağrılmıyor. |
| A12 üretim zamanı | **Kodda düzeltildi:** sabit adım artığı sıradaki işe aktarılıyor; çıktı kapasitesi yüzünden bloke olan tamamlanmış işler yer açılınca süreyi tekrar harcamadan deneniyor. |
| A14, A18, A36 hayatta kalma durumu | **Kısmen kapatıldı:** location değişimi revision artırıyor; profil, hava durumu ve survival katsayılarında finite/range ön kontrolleri genişletildi. Model katsayılarının tasarım dengelemesi ve runtime doğrulaması yapılmadı. |
| A15–A16 hacim/kütle | **Kodda güçlendirildi:** voxel hacim kestirimi grid köşe ağırlıklarını hesaba katıyor; kütle kestirimi taşıma ve diğer kod yollarında ortak mesh-bounds hacim/density fonksiyonuna geçti. AABB ve doluluk oranı hâlâ yaklaşık modeldir. |
| A22–A23 taşıma/depo | **Kodda güçlendirildi:** taş/çubuk taşıma ve ground placement owner/lifecycle/graph durumunu uzlaştırıyor; container kimliği kayıt sistemine bağlandı; fiziksel depo ortak storage kapasitesini denetliyor ve container açıkça yok edilince kayıtlı parçaları World owner'a döndürmeyi dener. Başarısız telafi coordinator'ı faulted yapar. |
| A24 kapı/pencere/gate açma | **Kodda güçlendirildi:** kesimler ön kontroller, graph ve ownership snapshot'ları ile geri alma yolu kullanıyor. Bu compensating rollback'dir; motor düzeyinde ACID/atomiklik garantisi veya runtime kanıtı değildir. |
| A25 yapı üstüne kütük koyma | **Kodda düzeltildi:** merkezdeki küçük küre yerine tam yönelimli mesh-bounds kutusu ile overlap kontrol ediliyor. |
| A26 fixed-step rollback | **Kodda düzeltildi:** ateş yakıt/yanma durumu coordinator'ın rollback snapshot'ına eklendi ve adım öncesi bütün heat kaynakları doğrulanıyor. |
| A27 identity duplicate/fail-open | **Yanlış pozitif düzeltildi:** mevcut registry duplicate identity'yi yeni GUID ile yeniden anahtarlıyor; Identity component'in kendi kayıt başarısızlığı da aktörü devre dışı bırakıyor. |
| A28 build-plan tamamlaması | **Kısmen kapatıldı:** eksik/tekrar kullanılan slot aktörleri reddediliyor; kızak malzemeleri için owner/lifecycle ve support bağımlılıkları ön doğrulanıyor; önce leaf parçalar tüketiliyor; çıktı container'ı üretilemezse plan silinmiyor ve spawn yeniden deneniyor. Çoklu kaynağın tek bir toplu ledger transaction'ı hâlâ yok. |
| A29 committed event | **Kodda güçlendirildi:** aynı event/operation/type/object set retry'ları retention penceresi içinde yeniden yayınlanmıyor; bounded cache dolunca en eski kayıt atılıyor. Event geçmişi kalıcı save log'u değildir; eviction sonrası eski retry yeniden yayınlanabilir. |
| A30 wildlife RNG | **Yanlış pozitif düzeltildi:** mevcut wildlife hareketi instance'a özel seeded `FRandomStream` ve sabit 0.1 sn adım kullanıyor; global `FMath::RandRange` çağrısı yok. |
| A32 hayvan ürünü | **Kodda düzeltildi:** üretim ilerlemesi [0, 3600) aralığına sınırlandı; tek sabit adımda büyüyebilen ürün adedi sınırlandırıldı. |
| A33 terrain receipts | **Kodda düzeltildi:** terrain operasyon receipt'leri save schema v14'e eklendi, bounded ve restore sırasında doğrulanıyor. |
| A34–A35 plan kesimi/custom length | **Kodda güçlendirildi:** gerekli kesim üretilemezse plan slotu yerleştirmesi reddediliyor ve başarısız yerleştirmede kesim/transform geri alınıyor; custom ölçekli uzunluklar inventory tarafından canonical bir uzunluk gibi yeniden yaratılmıyor. |
| A37 ledger retention | **Kısmen kapatıldı:** ownership item ledger terminal tombstone'ları kapasite gerektiğinde ayıklıyor; transfer/split receipt'leri ayrı bounded limitlerde sekansla tutuluyor; event dedupe cache de bounded. Genel WorldOperationLedger tamamlanmış kayıtları dolunca hâlâ yeni işlem açmayı reddediyor; güvenli eviction için operation-ID reuse/retention sözleşmesi gerekiyor. |
| A38 taşıyarak save | **Bilinçli fail-closed sınırlama:** save coordinator, owner ledger'da `Held` item varken kaydı reddediyor. Taşınan item/carry-holder ilişkisini save/load etmek henüz desteklenmiyor. Bu bir “held item sessizce orphan oluyor” hatası değil, açıkça reddedilen işlevsellik. |
| A39 graph root placed state | **Yanlış pozitif düzeltildi:** graph restore kodu root Stone ve Stick aktörleri için `RestoreRootStateFromGraph` çağrısını zaten içeriyor. |

### Hâlâ çözülmemiş büyük kapsamlar

- A03: snapshot mutasyonundan önce çapraz-domain ön doğrulama var; ancak bütün domain'ler için ortak staged commit/atomic swap yok. Bir domain-local doğrulama uygulama sırasında yine başarısız olursa eski snapshot telafi olarak yeniden uygulanır; telafinin kendisi başarısız olabilir ve simülasyon faulted olur.
- A04: dynamic actor spawn/destroy, saved GUID, component-local transform/physics restore, restore-aware EndPlay koruması ve ownership ledger'dan physical storage rebuild kodda var. Log/stone/stick connection cleanup eski graph'ı bozmasın diye restore sırasında bastırılıyor. Açık sınırlar: runtime Blueprint component düzeni ve BeginPlay yan etkileri, schema v17 öncesi özel child offset/scale kaybı, UHT/build/PIE doğrulamasının yapılmamış olması.
- A06: çoklu düğüm hasarı artık graph copy üzerinde tek atomik map-swap ile uygulanıyor; saldırıdan sonra gereken collapse listesi önceden doğrulanıyor. Collapse sırasında aktör yıkımı yine motor seviyesinde geri alınamaz; beklenmeyen hata pending pulse + fail-closed davranışla durdurulur. Tam AI, NavMesh, dalga kompozisyonu ve gerçek settlement target resolver hâlâ yok.
- A17/A18/A19/A20/A21/A31: eşit yük paylaşımı, survival katsayıları, çevresel tarım etkileri, kapasite birimleri, halat fiziği ve yapısal yük kapasitesi hâlâ prototip sözleşmeleri/heuristics.
- A09/A16/A24/A28/A29 için kısmi düzeltmeler gerçek mesh kesimi, paylaşılan batch transaction, kalıcı durable event log veya kanıtlanmış atomiklik anlamına gelmez.
- A37: genel operation ledger tamamlanmış kayıtları sınır dolunca reddediyor; yalnızca kalıcı domain makbuzu/watermark bulunan durumlarda güvenli budama yapılmalı.
- A38: taşınan item/carry-holder bağını tam save/load etmek hâlâ desteklenmiyor; Held item ile save açıkça reddediliyor.

### Güncel durumun okunması

Bu bölümdeki A01–A39 listesi ilk statik incelemenin bulgularını tarihsel olarak tutar. Hangi bulgunun bugün açık veya kısmen kapalı olduğu için **öncelikli kaynak 1A tablosu ve yukarıdaki “Hâlâ çözülmemiş büyük kapsamlar” güncellemesidir**. Aşağıdaki eski açıklamalar, kod daha sonra değişmiş olsa bile ilk bulgunun ne olduğunu anlatır; onları tek başına güncel açık-hata listesi olarak okumayın.

## 2. İlk denetimdeki en önemli bulgular (tarihsel bulgu listesi)

1. **P0 — Voxel density epsilon düzeltmesi işaret değiştirebiliyor.** Negatif ve sıfıra yakın değer +epsilon yapılabiliyor; bu, katı tarafındaki örnekleri hava tarafına çevirebilir.
2. **P0 — VerticalSlice_B oluşturma akışı ortak fixed-step simülasyon sürücüsünü kurmuyor.** Oyuncu aktivitesi kaydı var ama coordinator'ın simülasyon adımları bu akıştan çağrılmıyor.
3. **P0 — Save/load atomik değil.** Snapshot uygulaması sistemleri değiştirdikten sonra hata alırsa önceki canlı dünya durumunu geri yüklemiyor.
4. **P0 — Fiziksel aktör snapshot'ı değişmiş dünya nüfusunu yeniden kuramıyor.** Mevcut aktör kümesi ile kayıttaki kümenin birebir aynı olmasını bekliyor; yeni doğan/silinen dinamik aktörler restore kapsamına girmiyor.
5. **P0 — Yapısal grafikte ikincil destek kaldırma/reparent sonrası kalan desteklerin kapasitesi yeniden doğrulanmıyor.**
6. **P0 — Raid adapter, coordinator fixed-step akışında çağrılmıyor.** Mantıksal baskın kayıtları ile gerçek pawn spawn/hasar davranışı bağlantısız kalabilir.
7. **P1 — Kütük boyuna kesim reçeteleri sadece merkezlerin eksene hizasını kontrol ediyor.** Doğru merkez aralığı, uzunluk korunumu, boşluk ve örtüşme doğrulanmıyor.
8. **P1 — Mesh-set geometri tablosunda aynı uzunluk/şekil tuple'ı tekrarlanırsa ilk eşleşme seçiliyor.**
9. **P1 — Ağaç verimi devrilmiş mesh'in dünya Z yüksekliğinden belirleniyor; düşüş pozu verim bandını değiştirebilir.**
10. **P1 — Kütük uzunluk kısaltma, bazı yapı işlemlerinde gerçek kesim yerine mesh'i uzun eksende ölçekliyor.**
11. **P1 — Kütle tahmininin iki ayrı formülü var; taşıma, hasar ve diğer mekanikler aynı kütle sözleşmesini kullanmıyor.**
12. **P1 — Rota eşit maliyetli seçeneklerde açık deterministik tie-break kullanmıyor.**
13. **P1 — Fiziksel hasar girişleri NaN/sonsuz ve aralık yönünden tam korunmuyor; iki farklı damage API'si aynı çarpma yolunda çağrılıyor.**
14. **P1 — Hayatta kalma profili revision kuralları tutarsız; bazı durum değişiklikleri revision artırmıyor.**
15. **P2 — Tarım, iklim, kütle, yük taşıma, halat ve depo kapasitesi formüllerinin bir bölümü açıkça basitleştirilmiş prototip sezgiselleri.** Bunlar yanlış olmak zorunda değil; ancak birim ve kapsamları tek sözleşmede tanımlanmalı.

## 3. P0 — Sistem tamamlanmadan önce çözülmesi gereken algoritmik/entegrasyon eksikleri

### A01 — Voxel density epsilon işaretini ters çevirebilir
**Dosya:** Source/MyProject3/VoxelTerrainChunkComponent.cpp — AvoidIsoValue; InitializeFlatGround; ApplySphereEdit.

AvoidIsoValue, mutlak değeri epsilon'dan küçük olan her değeri +epsilon yapıyor. Density işaretinde negatif “katı” olduğundan -0.005 gibi bir değerin +0.01 yapılması yalnızca sıfır yüzeyinden uzaklaştırma değil, sınıfın değişmesi demektir. Bu, Marching Cubes yüzeyini ve kazı/ekleme sonuçlarını etkileyebilir.

**Gerekli ilke:** işareti koru; sıfıra yakın pozitif değer +epsilon, negatif değer -epsilon olmalı. Tam sıfır için politika ayrıca belgelenmeli.

### A02 — VerticalSlice_B ortak simülasyon sürücüsüne bağlanmıyor
**Dosyalar:** UnrealEditorScripts/CreateVerticalSliceBMap.py; UnrealEditorScripts/RebuildVerticalSliceBFromScratch.py; Source/MyProject3/VerticalSliceBWorldActor.cpp; Source/MyProject3/WorldDemoBootstrapActor.cpp; Source/MyProject3/WorldSimulationCoordinatorSubsystem.cpp.

Yeni harita betiği ışık, PlayerStart ve VerticalSliceBWorldActor oluşturuyor; WorldDemoBootstrapActor oluşturmuyor. VerticalSliceBWorldActor::Tick oyuncu konumunu ve yürüme/koşma aktivitesini kaydediyor ancak UWorldSimulationCoordinatorSubsystem::AdvanceFixedStep çağırmıyor. Başka bir bootstrap aktörü elle konulmadıkça ortak saat, bazal survival, tarım, üretim, cargo ve threat adımları bu map kurulumundan ilerlemiyor olabilir.

**Gerekli ilke:** tek ve açık simulation driver; world time, weather, survival, agriculture, production, cargo ve raid adımları aynı fixed-step sırasından çağrılmalı. Dikey kesit actor'ları kendi saatini oluşturmamalı.

### A03 — Save/load sıralı mutasyon ve telafi yolu

**Dosya:** Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp — ApplySnapshot; LoadWorldState; ApplyPhysicalActorSnapshots.

ApplySnapshot alt sistemleri sırayla uygular; tek başına başarısızlıktan önce yapılan değişiklikleri geri almaz. Önemli ayrıntı: LoadWorldState, yüklemeden önce mevcut durumu yakalar ve aday yükleme başarısız olursa bu durumu ApplySnapshot ile yeniden uygulamayı dener. Yani bir telafi yolu var; ancak bu ikinci restore da başarısız olabilir ve böyle bir durumda coordinator simülasyonu faulted olarak işaretler. Fiziksel aktör değişiklikleri de sırayla uygulanır.

**Gerekli ilke:** mümkün olan tüm domain doğrulamalarını mutasyon başlamadan önce yap; yüklemeyi aşamalı commit eden bir sözleşme tanımla. Mevcut telafi yolunu koru, fakat onu tek başına atomik işlem garantisi olarak değerlendirme.

### A04 — Dinamik fiziksel aktörler kayıt yüklemede yeniden oluşturulamıyor
**Dosya:** Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp — ValidatePhysicalActorSnapshotSet; ApplyPhysicalActorSnapshots.

Mevcut identity'li aktör kümesinin sayısı/kümesi snapshot ile birebir aynı olmalı. Kodda aktör spawn etmenin restore sırasında “tahmin edilmediği” de açıkça belirtiliyor. Ağaç devrilmesiyle üretilmiş kütükler, tüketilmiş nesneler veya sonradan spawn edilmiş aktörler taze haritada aynı kimliklerle yoksa kayıt uygulanamayabilir.

**Gerekli ilke:** actor spawn/destroy manifesti ve canonical definition üzerinden eksik aktörleri üretme, fazla aktörleri kaldırma veya bu kapsamdaki sınırlamayı bilinçli biçimde sabitleme. Mevcut yaklaşım tam dinamik sandbox restore'u değil, aynı aktör manifestini geri uygulama yaklaşımıdır.

### A05 — İkincil destek kaldırma/reparent kapasiteyi tekrar hesaplamıyor
**Dosya:** Source/MyProject3/WorldStructuralGraphSubsystem.cpp — TryRemoveSecondarySupport; UnregisterNode; ComputeAggregateLoad.

TryRemoveSecondarySupport bağlantıyı silip revision artırıyor; kalan desteklerin yeni aggregate load değerlerini MaxSupportedLoad ile karşılaştırmıyor. UnregisterNode da secondary support'u parent olarak yükseltebiliyor ancak yeni destek zincirinin yük kapasitesini doğrulamıyor. Support path mevcut görünürken yük limitinin aşılması mümkün.

**Gerekli ilke:** her support add/remove/reparent işlemi, grafiğe commit edilmeden önce etkilenen düğümlerin support path ve aggregate load değerlerini hesaplamalı; başarısız işlem mutasyonsuz kalmalı.

### A06 — Baskın uygulama adapter'ı coordinator tarafından çalıştırılmıyor
**Dosyalar:** Source/MyProject3/WorldSimulationCoordinatorSubsystem.cpp; Source/MyProject3/WorldThreatDirectorSubsystem.cpp; Source/MyProject3/WorldRaidAdapterSubsystem.cpp.

ThreatDirector baskın uyarısı/başlangıcı/sonu gibi mantıksal kayıtları ilerletiyor. RaidAdapter ise gerçek Pawn spawn ve savunma düğümlerine hasar pulse'ları uyguluyor; coordinator'ın fixed-step fonksiyonunda RaidAdapter çağrısı yok. Mantıksal raid durumu, sahnedeki düşman ve hasar akışına kendi başına dönüşmüyor.

Ayrıca TargetLocation, oyuncu üssüne, world bounds'a veya nav adaylarına göre seçilmek yerine origin çevresindeki sabit bir XY karesinden rastgele üretiliyor. Bu yalnızca test hedef noktası üretimi; gerçek baskın konumlandırması değil.

## 4. P1 — Matematiksel veya algoritmik sözleşmesi eksik mekanikler

### A07 — Boyuna kesim reçetesi fiziksel uzunluk korunumu kanıtlamıyor
**Dosya:** Source/MyProject3/LogBuildPiece.cpp — IsValidLengthCutLayout; TryResolveLengthCutVariant; TrySplitLengthOutputPieces.

IsValidLengthCutLayout iki merkezin ayrışmasını ve ayrışma yönünün kütük eksenine hizasını kontrol ediyor; merkez aralığının çıktı uzunluklarına uygunluğunu, mesh sınırlarının bitişik olduğunu veya örtüşmediğini doğrulamıyor. Yanlış ofset reçetesi hizalama şartını geçebilir.

**Gerekli ilke:** çıktı mesh uzunlukları ve ölçeklerinden beklenen merkez mesafesini türet; aralık, boşluk/örtüşme toleransı ve kesim konumunu açıkça doğrula.

### A08 — Mesh-set tablolarında duplicate geometri mapping belirsizliği
**Dosya:** Source/MyProject3/LogBuildPiece.cpp — TryResolveGeometryVariant.

ProcessingMeshSet yolunda aynı LengthFraction + CrossSectionProfile için birden çok kayıt varsa fonksiyon ilkini döndürüyor. Diğer GeometryVariantMeshes yolunda birden çok eşleşme reddediliyor. Böylece set tablosunda dizi sırası sonucu belirleyebilir.

**Gerekli ilke:** tuple başına tam bir geçerli mesh; sıfır veya birden çok eşleşme fail-closed olmalı.

### A09 — Yapı açıklığı işlemleri gerçek kesilmiş geometri üretmiyor
**Dosyalar:** Source/MyProject3/ConstructionHelpers.cpp — TrimLogToRange; Source/MyProject3/LogBuildToolsComponent.cpp — CutOpening.

TrimLogToRange gerçek kesim yapmayıp actor scale'ini kütük ekseninde değiştiriyor. Bu, uzunluğu yaklaşık değiştirebilir ama gerçek kesit yüzeyi/end-grain, fiziksel hacim ve collision geometrisini garanti etmez. CutOpening bazı yatay kütükleri Z merkez noktasına göre seçiyor; kesitin yarıçapını açıklık sınırına katmadığından sınırdaki kütük uçları açıklığa taşabilir.

**Gerekli ilke:** prototipte ölçekleme kullanımı “approximate trim” olarak açık tanımlansın; gerçek kesim için exact mesh varyantı veya geometri üretimi tercih edilsin. Açıklık testi kütüğün eksen aralığı ve kesit yarıçapını hesaba katsın.

### A10 — Ağaç verimi devrildikten sonraki dünya Z boyutundan seçiliyor
**Dosya:** Source/MyProject3/HarvestableTreeActor.cpp — SpawnHarvestLogs.

YieldRules seçimi TreeMesh->Bounds.BoxExtent.Z * 2 ile yapılıyor. Fonksiyon ağacın yere düşmesinden sonra çağrıldığı için bu dünya Z boyutu düşüş açısına bağlı. Aynı ağaç farklı pozda farklı log yield bandına düşebilir.

**Gerekli ilke:** devrilmeden önce kaydedilen standing height veya mesh gövde ekseni boyunca yerel ölçü kullanılmalı.

### A11 — Rota eşit maliyetli yollar için açık tie-break kullanmıyor
**Dosya:** Source/MyProject3/WorldLogisticsRouteSubsystem.cpp — FindRoute.

Dijkstra yaklaşımı küçük graf için yeterli; ancak eşit Distance değerlerinde düğüm seçimine ikinci bir kararlı kural eklenmemiş. Düğümler TSet üzerinden dolaşıldığından eşit maliyetli alternatiflerde rota seçimi açık bir deterministik sözleşmeye bağlı değil.

**Gerekli ilke:** stable NodeId/edge sıralaması ile tie-break kullan; Edge Cost'un birimini Cargo CostPerSecond ile tutarlı biçimde belgele.

### A12 — Üretim adımı fazla simülasyon zamanını kaybedebilir
**Dosya:** Source/MyProject3/WorldProductionSubsystem.cpp — AdvanceProductionStep.

Job.ProgressSeconds, DurationSeconds ile sınırlandırılıyor ve iş bittiğinde station döngüsü break ediyor. DeltaSeconds kalan job süresinden büyükse fazladan zaman sıradaki queued işe aktarılmıyor. Fixed-step ve recipe duration seçimine göre üretim throughput'u değişebilir.

**Gerekli ilke:** recipe sürelerini fixed-step ile kısıtla veya adımın kalan süresini sıradaki işe taşı.

### A13 — Hasar formülünde finite/range koruması eksik ve iki damage yolu çağrılıyor
**Dosyalar:** Source/MyProject3/PhysicalHealthComponent.cpp — ApplyPhysicalDamage; Source/MyProject3/ThrownImpactComponent.cpp — ApplyImpactTo.

Kinetik enerji temelli hasar formülünde Speed cm/s'ten m/s'e çevriliyor; birim dönüşümü kendi içinde tutarlı. Fakat hız, kütle, yön ve sonuç hasar için NaN/sonsuz/üst sınır kontrolleri tam değil. Amount <= 0 kontrolü NaN'i güvenilir biçimde reddetmez. Aynı çarpma yolu hem PhysicalHealthComponent hem UGameplayStatics::ApplyDamage çağırıyor; generic TakeDamage/Blueprint de hasar uygularsa çift hasar oluşabilir. Damage için tek authoritative çağrı noktası tanımlanmalı.

### A14 — Revision/state contract'ı tutarsız
**Dosyalar:** Source/MyProject3/WorldSurvivalSubsystem.cpp; Source/MyProject3/WorldObjectIdentityComponent.cpp.

Survival UpdateLocation profili değiştiriyor ama revision artırmıyor; SetWeather global state değiştiriyor ama revision/operation kaydı yok. Identity TryTransitionLifecycle StateRevision artırıyor ancak MAX_int64 sınırı kontrol edilmiyor; SetDefinitionId revision davranışını tanımlamıyor.

**Gerekli ilke:** revision hangi alanların değişikliklerini sayıyor netleştirilmeli; optimistic concurrency kullanan alanlar için tüm mutasyonlar aynı kurala uymalı.

### A15 — Voxel hacim hesabı örnek noktalarını hücre gibi sayıyor
**Dosya:** Source/MyProject3/VoxelTerrainChunkComponent.cpp — GetSolidVolumeCubicMetres; ApplySphereEdit.

Hacim, negatif density örneklerinin sayısı × voxel cell volume ile tahmin ediliyor. Density dizisi hücre merkezleri değil grid noktaları tuttuğundan bu kesin katı hacim değildir ve yüzey sınırında sistematik sapma oluşturabilir. Header'da approximate olarak nitelenmiştir; görsel prototip için uygun olabilir ama doğru kazı maliyeti/kaynak verimi için yeterli değil.

Tek chunk sınırlarında komşu density halo/senkronizasyonu, seam ve persistence da yok; WorldTerrainStateSubsystem cell ledger'ı gerçek voxel density grid'iyle bağlantılı değil.

## 5. P2 — Prototip heuristiği olarak kalabilecek yaklaşık modeller

### A16 — Kütle tahmini tek bir otorite değil ve AABB tabanlı
**Dosyalar:** Source/MyProject3/CarryLoadComponent.cpp — EstimateMassKg; Source/MyProject3/ConstructionHelpers.cpp — EstimateMassKg.

İki ayrı hesap var: mesh bounding-box hacmi × BoundsFillFactor × malzeme yoğunluğu ve actor bounds hacmi × sabit doluluk/yoğunluk katsayıları. Taşıma hızı, fırlatma hasarı ve diğer sistemler farklı kütle tahminleri kullanabilir. AABB yuvarlak kütük, yarım kesit, oluklu parça veya içi boş şekillerin gerçek hacmine eşit değil.

**Gerekli ilke:** merkezi mass/volume API'si; açık cm/m³/kg birim sözleşmesi; profile/material bazlı hacim modeli.

### A17 — Yapı yükü eşit paylaşım sezgiseli
**Dosya:** Source/MyProject3/WorldStructuralGraphSubsystem.cpp — ComputeAggregateLoad.

Birden fazla desteğe bağlı parçanın yükü destek sayısına göre eşit bölünüyor. Bu deterministik prototip heuristiğidir; açıklık, kaldıraç momenti, ağırlık merkezi, temas alanı, malzeme rijitliği ve kiriş eğilmesini çözmez. Kapasite grafı gerçek yapı mukavemeti simülasyonu olarak yorumlanmamalı.

### A18 — Hayatta kalma/iklim formülleri gameplay katsayılarıdır
**Dosya:** Source/MyProject3/WorldSurvivalSubsystem.cpp.

Weather delta, wind chill, clothing insulation, shelter protection, wetness, calorie loss ve health loss doğrusal katsayılarla birleştiriliyor. Bu deterministik bir oyun modeli olabilir; fiziksel/medikal gerçeklik modeli değildir. Katsayılar gameplay tuning olarak sınıflandırılmalı ve tek parametre sözlüğünde tutulmalı.

### A19 — Tarım ve hayvan modelleri çevre bağlantısı olmayan basit simülasyon
**Dosya:** Source/MyProject3/WorldAgricultureSubsystem.cpp.

Crop growth moisture'a lineer bağlanıyor; hayvan açlığı, sağlık ve ürün üretimi sabit saatlik kurallarla ilerliyor. Mevsim, sıcaklık, toprağın besinleri, tür farklılıkları, habitat/üreme ve ürünün envantere teslimi bu hesaplara bağlı değil. Bu test eksiği değil, tanımlanmış oyun modelinin henüz tamamlanmamış olması.

### A20 — Depo ve üretim kapasitesi soyut birim sayısı
**Dosyalar:** Source/MyProject3/WorldStorageSubsystem.cpp; Source/MyProject3/WorldProductionSubsystem.cpp.

CapacityUnits item Quantity üzerinden sayılıyor. 1 uzun kütük ile 1 küçük nesne aynı kapasiteyi kaplamamalıysa bu fiziksel lojistik modeli değildir. Soyut bir kapasite kuralı olarak kullanılacaksa tanımlanmalı; fiziksel depolama hedefleniyorsa item definition hacim/kütle taşımalı.

### A21 — Halat hareketi gerçek kablo mekaniği değil
**Dosya:** Source/MyProject3/RopeLineActor.cpp — Tick.

Hız sin(slope) ile türetilen yerçekimi bileşeniyle artıyor; Rider.Distance artışında en az 80 cm/s zorlanıyor. Bu, net ivme negatif olsa bile rider'ın ilerlemesini sağlar. Düşük maliyetli traversal prototipi olabilir ama kablo gerilimi, sallanma, sürtünme ve yük kütlesi için fiziksel bir çözüm değildir.

## 6. Birleştirilmesi gereken ortak sözleşmeler

1. **Geometri:** length/profile → mesh → pivot/longitudinal axis → allowed operation → output centers → collision/mass.
2. **Dünya saati:** tek clock/fixed-step → survival + heat fuel + agriculture + production + cargo + raid adapter.
3. **Fizik ve yapı:** tek mass/volume owner → carry burden/impact/structural load; her support değişikliği yeni kapasite hesabına tabi.
4. **Kaynak sahipliği:** pickup/cut/production/storage/cargo/craft/save-load aynı ItemId/OwnerKind/Revision sözleşmesini paylaşmalı.
5. **Kalıcılık:** logical snapshots yanında dinamik aktör doğma/silinme, voxel chunk density/edit, taşıma ve bileşen durumları için restore sözleşmesi gerekli.
6. **Baskın:** schedule → target selection → spawn/nav/attack → structural damage → collapse aynı simülasyon sırası içinde yürümeli.

## 7. Önerilen düzeltme sırası

1. Voxel density epsilon işaretini koru ve hacim değerinin approx olduğunu API seviyesinde belirt.
2. Geometri mesh-set duplicate tuple kontrolü ile boyuna kesim merkez/uzunluk aralığı doğrulamasını tamamla.
3. Ağaç yield hesabını devrilme öncesi ölçüye bağla; TrimLogToRange'ın yaklaşık ölçekleme mi gerçek kesim mi olduğunu netleştir.
4. Kütle/yoğunluk hesabını birleştir; hasar girişlerini finite/range açısından koru ve damage tek otoriteye indir.
5. Support remove/reparent için kapasite preflight ekle.
6. VerticalSlice_B map yaratma yoluna açık bootstrap ekle; raid adapter'ını coordinator'a bağla.
7. Save/load için staged validation + rollback ve dinamik aktör/voxel chunk restore modeli belirle.
8. Tarım, hayvancılık, iklim ve lojistik formüllerini tek dünya simülasyon sözleşmesine bağla.

## 8. Hariç tutulan doğrulamalar

Bu rapor hiçbir Unreal build, PIE veya otomatik test sonucu iddia etmez. M001–M119 mekaniklerini tamamlandı saymaz. Yaklaşık survival/load/mass formüllerinin hepsinin kaldırılması gerektiğini de iddia etmez; bazıları prototip için uygundur. Eksik olan, formül ve varsayımların ortak, açık ve tek bir sözleşme hâline getirilmesidir.


## 6. İkinci geçişte bulunan durum/sahiplik sözleşmesi eksikleri

### A22 — Taş ve çubukta fiziksel Held/Placed durumu, kimlik lifecycle state'inden ayrışabiliyor
**Dosyalar:** Source/MyProject3/LogCarryComponent.cpp — TryPickupMaterialPiece; Source/MyProject3/StoneBuildingPiece.cpp — SetHeldState/TryCommitGroundPlacement/TryConnectToAnchor; Source/MyProject3/StickPrimitivePiece.cpp — SetHeldState/TryCommitGroundPlacement/TryConnectToAnchor.

Taş/çubuk pickup, ledger OwnerKind değerini World → Held değiştiriyor; fakat bu kod yollarında UWorldObjectIdentityComponent::TryTransitionLifecycle çağrısı yok. SetHeldState sadece actor içindeki bIsHeld/bIsPlaced ve physics profilini değiştiriyor. Ground placement ve anchor bağlantıları da local bIsPlaced durumunu güncelliyor fakat identity lifecycle state'ini World → Placed yapmıyor. Kütük taşıma yolunda ise lifecycle geçişleri açıkça yapılıyor. Böylece aynı kavram, malzemeye göre iki farklı otorite ve sözleşmeyle temsil ediliyor.

**Gerekli ilke:** her fiziksel parça türü aynı lifecycle geçiş politikasını uygulamalı; ledger OwnerKind ile actor lifecycle birbirinden farklı kavramlar olarak tutulacaksa hangi kombinasyonların geçerli olduğu merkezi olarak doğrulanmalı.

### A23 — Sled/rack deposu kapasite ve yapısal uygunluğu tüm malzemelerde aynı kontrol etmiyor
**Dosyalar:** Source/MyProject3/LogContainerActor.cpp — CanAccept; TryStore; BeginPlay; Source/MyProject3/LogContainerActor.h.

CanAccept, yapıya bağlı/yerleştirilmiş kütükleri reddediyor; ancak AdvancedSled için stone/stick parçasını kabul ederken aynı placed/attached kontrolünü uygulamıyor. TryStore sonrasında Stone/Stick::SetHeldState(false) çağrısı, parça zaten held değilse true dönebiliyor; yerleştirilmiş parçayı destek grafiğinden ayırmak yerine fiziksel olarak sled'e attach edip graph bağlantılarını bırakma riski var. Depolama önkoşulu bütün IStructuralPieceInterface türlerinde ortak olmalı ve parent/child/support bağları olan parça depoya alınmamalı.

Ek olarak StorageId BeginPlay'de boşsa yeni FGuid ile üretiliyor, Stored dizisi SaveGame değil ve ALogContainerActor bu StorageId'yi UWorldStorageSubsystem'e kaydeden bir çağrı yapmıyor. Bu fiziksel rack/sled modeli ile genel storage/cargo modeli arasında iki ayrı depo otoritesi yaratıyor; dinamik saklanan parçaların kayıt sonrası aynı depo listesine ve aynı stabil StorageId'ye geri bağlanma sözleşmesi yok.

**Gerekli ilke:** fiziksel container, ownership ledger ve WorldStorageSubsystem arasında tek bir storage kimliği/üyelik kaydı olmalı; store/withdraw sırasında anchor ve structural graph geçişleri ya birlikte commit edilmeli ya da hiçbiri gerçekleşmemeli.



## Ek bulgular: kompozit işlemler ve ortak durum

### A24 — Kapı/pencere kesimi tek bir atomik işlem değil
**Dosyalar:** Source/MyProject3/LogBuildToolsComponent.cpp (CutOpening); Source/MyProject3/ConstructionHelpers.cpp (TrimLogToRange).

CutOpening birden fazla kütük üzerinde parça üretme, kaynak mesh'i ölçekleme ve parçayı kaldırma adımlarını arka arkaya yapıyor. Bazı alt işlemlerin sonuçları kontrol edilmiyor. Bir parçanın oluşturulması veya kaldırılması başarısız olursa önceki değişiklikleri geri alan ortak transaction bulunmuyor. Bu nedenle hata yolunda malzeme kaybı veya fazladan parça kalması mümkün.

**Gerekli ilke:** işlem öncesi tüm adımları doğrula; sonra hepsini birlikte commit et; başarısızlıkta tüm önceki transform/mesh/durum ve yeni aktörleri geri al.

### A25 — Stack çakışma denetimi tüm kütük hacmini temsil etmiyor
**Dosya:** Source/MyProject3/LogCarryComponent.cpp (ComputeStackPlacement).

Yerleşim adayı yalnızca üst parçanın merkezinde, Upper.Radius değerinin yarısı büyüklüğündeki bir sphere ile sorgulanıyor. Bu şekil uzun ve yönelmiş bir kütüğün tamamını temsil etmediğinden bazı kesişmeleri kaçırabilir. Ayrıca CommitHeldLogAt, birincil desteğe grafik bağlantısı kurulamazsa bile yerleştirmeyi tamamlayabilir; fiziksel yerleştirme ile yapısal bağlantı sonucu ayrışabilir.

**Gerekli ilke:** tam collision şekli veya yönelmiş eksen/yarıçap geometrisiyle çakışma kontrolü; destekli yerleştirmede grafik bağlantısını başarı koşulu yapmak.

### A26 — Fixed-step rollback ısı kaynağı bileşen durumunu kapsamıyor
**Dosyalar:** Source/MyProject3/WorldSimulationCoordinatorSubsystem.cpp; Source/MyProject3/WorldSurvivalSubsystem.cpp; Source/MyProject3/HeatSourceComponent.cpp.

Survival adımı heat source yakıtını ilerletiyor. Coordinator geri alma sırasında profil ve alt sistem snapshot'larını geri alıyor ancak HeatSourceComponent yakıt/yanma alanları aynı transaction snapshot'ına dahil değil. Bir adım sonradan başarısız olursa profil ve saat geri alınırken yakıt ilerlemiş kalabilir.

**Gerekli ilke:** fixed-step sırasında değişen her durum sahibi rollback snapshot'ına dahil edilmeli veya durum step commit edilene kadar geçici tutulmalı.

### A27 — Identity registry duplicate/failure davranışı: ilk bulgu düzeltilmiş olarak işaretlendi
**Dosyalar:** Source/MyProject3/WorldObjectIdentityComponent.cpp (BeginPlay); Source/MyProject3/WorldObjectIdentitySubsystem.cpp (RegisterIdentity).

İlk not duplicate ID'yi aktör etkin bırakma riski olarak kaydetmişti. Kaynak yeniden kontrol edildiğinde, registry çakışan instance ID'yi yeni GUID ile değiştiriyor; component kayıt gerçekten başarısız olduğunda da aktörü devre dışı bırakıyor. Bu nedenle ilk bulgu mevcut kod için yanlış pozitiftir. Herhangi bir runtime deneyi yapılmadı.


## Ek bulgular: build-plan, event ve deterministik dünya

### A28 — Build-plan tamamlanması çok parçalı commit olarak korunmuyor
**Dosya:** Source/MyProject3/BuildPlanActor.cpp — Complete; MarkFilled.

LogSled planı tamamlanırken her bileşen için ConsumePiece sonucu kontrol edilmiyor; girdiler tüketildikten sonra container spawn edilmeye çalışılıyor. Container spawn başarısız olsa bile plan en sonda Destroy ediliyor. Birden fazla tüketimden biri başarısız olduğunda da işlem kalan parçalarla devam ediyor. Böylece blueprint tamamlanması bütün girdi/çıktı değişimini atomik tutmuyor.

**Gerekli ilke:** tüm slot girdilerini, destek bağlarını ve container spawn olanağını preflight et; girdi tüketimi ve çıktı oluşturmayı tek transaction'a al; hata halinde tüm girdiler/durumlar geri gelsin. MarkFilled da geçerli parça ve başarılı bağlantı sonucuna bağlı olmalı.

### A29 — Committed event publisher: bounded deduplication window

**Dosya:** Source/MyProject3/WorldCommittedEventSubsystem.cpp — PublishCommittedEvent.

İşlem ID'si/type/affected-object set'i normalize edilip retention penceresi içinde tekrar yayınlanmaz; aynı EventId farklı bir fact için kullanılırsa reddedilir. En eski event kayıtları `MaxRetainedEvents` sınırında deterministik olarak atılır, böylece event yayıncısı sonsuza kadar durmaz.

**Sınır:** Bu kalıcı event log'u değildir. Cache'ten düşmüş çok eski retry'lar yeniden yayınlanabilir. Uzun vadeli tüketiciler tam exactly-once gerektiriyorsa persistent event store/outbox veya tüketici tarafında kalıcı dedup gerekir.

### A30 — Wildlife hareketi global RNG ve ayrı actor tick'i kullanıyor
**Dosya:** Source/MyProject3/WildlifeActor.cpp — Tick.

Hayvan hedefleri FMath::RandPointInCircle ve FMath::FRandRange ile global RNG'den seçiliyor; M006'da planlanan dünya seed/operation stream'ine bağlı değil. Actor ayrıca kendi DeltaSeconds tick'iyle hareket ediyor; bu hareket state'i ortak fixed-step snapshot/restore sisteminde görünmüyor. Aynı world seed ve yükleme durumundan aynı hayvan davranışını yeniden üretmek mümkün değil.

**Gerekli ilke:** world-seed tabanlı per-entity random stream, açık simülasyon adımı ve hareket durumunun snapshot/restore sahibi tanımlanmalı. Eğer vahşi yaşam deterministik olmayacaksa bu ayrı, belgelenmiş bir istisna olmalı.


### A31 — Yapısal yük/hasar girdileri tüm malzeme türlerinde aynı sabitlerle başlıyor
**Dosyalar:** Source/MyProject3/LogBuildPiece.cpp; Source/MyProject3/StoneBuildingPiece.cpp; Source/MyProject3/StickPrimitivePiece.cpp; Source/MyProject3/WorldStructuralGraphSubsystem.h.

Üç fiziksel parça ailesi de graph kaydını `LocalLoad=1.0`, `MaxSupportedLoad=100000.0`, `DamageThreshold=100.0` ile başlatıyor. `ComputeAggregateLoad` bu girdileri matematiksel olarak toplasa da girdiler uzunluk, kesit profili, gerçek kütle veya malzeme dayanımından türetilmiyor. Taş sütun ve ince çubuk aynı yerel yük ve kapasite parametrelerini alıyor; bu yüzden grafın yük hesabı sayısal olarak çalışabilir ama fiziksel anlamı tasarım kurallarıyla ilişkilendirilmiş değil. Hasar eşiği de malzeme/şekil bazında farklılaşmıyor.

**Gerekli ilke:** bir ortak fiziksel profil tanımla: `MassKg`/`LocalLoad`, izin verilen basma/eğilme/ankraj kapasitesi, dayanıklılık ve hasar eşiği. Bu değerler mesh AABB'sinden gelişigüzel türetilmek yerine, canonical dimensions + profile + material data üzerinden hesaplanmalı; prototip sabitleri açıkça “placeholder tuning” olarak etiketlenmeli.


### A32 — Hayvan ürün ilerlemesi için üst sınır ve sabit-zamanlı hesap yok
**Dosya:** Source/MyProject3/WorldAgricultureSubsystem.cpp — RegisterAnimal, CanAdvanceAgricultureStep, AdvanceAgricultureStep, RestoreSnapshot.

ProductProgressSeconds yalnızca sonlu ve negatif olmayan değer olarak doğrulanıyor; üst sınırı yok. AdvanceAgricultureStep, her 3600 saniyelik ürün birimi için bir `while` iterasyonu çalıştırıp ProductUnits değerini artırıyor ve ancak MAX_int32 değerine ulaştığında duruyor. Kayıt veya snapshot büyük fakat sonlu bir ilerleme değeri içerirse tek bir simülasyon adımı aşırı sayıda iterasyon yapabilir. CanAdvanceAgricultureStep bu durumu mevcut kontrolleriyle engellemiyor.

**Gerekli ilke:** ürün artışını quotient/remainder aritmetiğiyle hesapla; üretim adedini MAX_int32 - ProductUnits ile sınırla; kayıt/yüklemede ürün ilerlemesi için canonical bir üst sınır uygula.


### A34 — Build-plan yuvası için kütük kısaltma sonucu zorunlu tutulmuyor
**Dosyalar:** Source/MyProject3/LogBuildToolsComponent.cpp (TryFillPlanSlot); Source/MyProject3/ConstructionHelpers.cpp (CutOffRemainder, TrimLogToRange).

TryFillPlanSlot, parça hedef boydan %10'dan fazla uzunsa önce CutOffRemainder çağırıyor; fakat bu çağrı sonuç üretmezse de yerleştirmeye devam ediyor. Böylece hedef boy koşulu garanti edilmiyor. Kısaltma başarılı olduktan sonra CommitHeldLogAt başarısız olursa, önceki kesim ve artan parça üretimi geri alınmıyor. CutOffRemainder içindeki trim dönüş değerleri de kontrol edilmiyor.

**Gerekli ilke:** trim/spawn sonuçları zorunlu kontrol edilmeli; kesim ve yuvaya yerleştirme tek bir işlem olarak ele alınmalı; yerleştirme commit'i başarısızsa önceki geometri ve çıktı aktörleri geri alınmalı.


### A33 — Arazi düzenleme receipt'leri save/load geri yüklemesinde kayboluyor
**Dosya:** Source/MyProject3/WorldTerrainStateSubsystem.cpp — ExportSnapshot, RestoreSnapshot, TryApplyCellEdit.

Terrain snapshot yalnızca hücre durumlarını dışa aktarıyor. RestoreSnapshot hücreleri geri yükledikten sonra `Receipts.Reset()` çağırıyor; operation receipt'leri snapshot'a dahil değil. Bu nedenle kayıt geri yüklendiğinde geçmişte tamamlanmış bir edit operation ID'si için idempotent sonuç kaydı kalmıyor. Aynı operation ID tekrar geldiğinde davranış artık önceki commit receipt'ine göre değil, yalnızca yeni beklenen revision değerine göre belirleniyor.

**Gerekli ilke:** terrain cell ve edit receipt'lerini aynı sürümlü snapshot'a dahil et; load sonrası aynı operation ID aynı sonuç kaydına bağlanmalı veya açıkça reddedilmeli.


### A35 — Ölçekle kısaltılan kütüğün canonical item definition kaydı değişmiyor
**Dosyalar:** Source/MyProject3/ConstructionHelpers.cpp — TrimLogToRange/CutOffRemainder; Source/MyProject3/WorldItemOwnershipSubsystem.cpp; Source/MyProject3/InventoryCraftingComponent.cpp — physical-item drop/variant restore.

TrimLogToRange kütük uzunluğunu actor scale'i üzerinden değiştiriyor; fakat bu işlem ilgili item'ın canonical DefinitionId/BaseDefinitionId/LengthFraction bilgisini ve ownership kaydının mesh metadata'sını aynı geometriyle eşleştirmiyor. Sonradan envantere girip world actor olarak yeniden üretildiğinde InventoryCraftingComponent, kayıtlı definition/mesh variant'tan hazırlanmış mesh'i tekrar seçiyor; kesilerek ölçeklenmiş gerçek uzunluk kaybolabilir veya bir daha farklı boyda ortaya çıkabilir.

**Gerekli ilke:** ya kesilmiş uzunluğu canonical tanımlı bir geometri varyantına dönüştür ve ledger'ı atomik güncelle, ya da “custom dimension” alanını kalıcı veri olarak saklayıp yeniden üretimde actor scale/geometriyi aynen geri kur. Sadece transform ölçeği fiziksel parçanın kalıcı kimliği olamaz.


### A36 — Hayatta kalma adımı kullandığı tüm global katsayıları doğrulamıyor
**Dosya:** Source/MyProject3/WorldSurvivalSubsystem.cpp — CanAdvanceSurvivalStep; AdvanceSurvivalStep; ComputeFeltTemperature.

CanAdvanceSurvivalStep bazı ayarları (DeltaSeconds, hava şiddeti, açlık/susuzluk/yorgunluk katsayıları, soğuk hasarı, kalori kapasitesi ve ambient sıcaklık) doğruluyor; fakat hesapta doğrudan kullanılan ClothingWarmthCelsius, ShelterWarmthCelsius, WetnessPenaltyCelsius, ComfortTemperatureCelsius, HypothermiaThresholdCelsius, CoreCoolingPerDegreeHour, CoreRecoveryPerHour, ShiverKcalPerDegreeHour ve CarryKcalPerKgHour gibi diğer ayarların tümünü doğrulamıyor. Bu değerlerden biri finite değilse preflight başarılı görünürken felt temperature/core temperature/kalori hesabına NaN veya infinity girebilir.

**Gerekli ilke:** her state-changing step'te kullanılan tüm global parametreler için tam finite/range sözleşmesi oluştur; doğrulanmayan konfigürasyonla hiçbir profil mutasyonu başlatma.


### A37 — Ledger kapasitesi ve retention: item ownership bounded, operation ledger remains fail-closed

**Dosyalar:** Source/MyProject3/WorldOperationLedgerSubsystem.h/.cpp; Source/MyProject3/WorldItemOwnershipSubsystem.h/.cpp; Source/MyProject3/WorldCommittedEventSubsystem.h/.cpp.

Ownership ledger artık terminal `Consumed/Transformed` tombstone'larını yalnız yeni item alanı gerektiğinde deterministik biçimde temizleyebiliyor. Transfer/split retry receipt'leri ayrı limitlerde saklanıyor; sequence en eskiden başlayarak eviction sağlıyor. Save schema v15, eski receipt'lere deterministik sequence veriyor ve restore hem receipt count limitlerini hem de sequence benzersizliğini doğruluyor. Committed-event publisher da bounded in-memory dedup penceresi kullanıyor.

**Kalan kasıtlı sınırlama:** `UWorldOperationLedgerSubsystem` 8192 tamamlanmış/pending giriş sınırında yeni operation'ı reddediyor. Tamamlanmış operation girişini güvenle atmak, aynı ID'li eski retry'ın yeni bir mutation gibi yeniden başlamasını mümkün kılabilir. Bunu çözmek için üretici tarafında operation ID expiry/epoch sözleşmesi veya kalıcı arşiv/checkpoint katmanı gerekir; bu sözleşme şu anda yok.

**Gerekli ilke:** item/event retention için bounded windows kullan; mutation sonucunu idempotent tutması gereken operation ledger'ı ise açık retry-lifetime policy olmadan otomatik atma.


### A38 — Held item kaydı bilerek reddediliyor; taşıyarak kaydetme henüz desteklenmiyor

**Dosyalar:** Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp; Source/MyProject3/LogCarryComponent.h/.cpp; Source/MyProject3/CarryLoadComponent.h/.cpp.

Önemli düzeltme: Save coordinator, ownership ledger içinde OwnerKind=Held olan herhangi bir item bulursa kaydı açıkça reddediyor. Dolayısıyla mevcut kodun “Held item kaydedip sahipsiz bırakması” diye bir iddia doğru olmaz; şu an kaydetme işlemi fail-closed davranıyor. Ancak bu, oyuncu elinde kütük/taş/çubuk varken kayıt almanın henüz desteklenmediği anlamına geliyor.

**Gerekli ilke:** Prototip aşamasında “önce nesneyi bırak veya depola” kuralı kabul edilebilir ve güvenlidir. Daha sonra taşıyarak kaydetme desteklenecekse, carrier ID + held item ID + taşıma modu/transform ilişkisi Save/Restore kapsamına eklenmeli.


### A39 — Root Stone/Stick placed-state restore: ilk bulgu yanlış pozitif çıktı
**Dosyalar:** Source/MyProject3/WorldStructuralGraphSubsystem.cpp — RebuildLogActorAttachmentsFromSnapshot; Source/MyProject3/StoneBuildingPiece.cpp; Source/MyProject3/StickPrimitivePiece.cpp.

Kaynak tekrar kontrol edildiğinde `RebuildLogActorAttachmentsFromSnapshot`, parent edge'i olmayan root taş ve çubuklar için `RestoreRootStateFromGraph(bGroundSupported, bStorageOwned)` çağrısını zaten içeriyor. Bu nedenle ilk notta belirtilen eksik mevcut kodda görülmedi. Runtime doğrulaması yapılmadı; kalan save/load riskleri A03 ve A04 altında tutuluyor.

## 1B — Follow-up implementation status — 29 October 2026

**Authority:** This table supersedes the earlier current-status descriptions in §1A and the unresolved-summary bullets for A05/A06/A09/A18/A19/A20/A31/A37/A38 below. Older findings further down remain the historical reason the work was opened. This update is based on source changes only, not an Unreal build or runtime test.

| Finding | Current source-level status |
|---|---|
| A05/A17/A21 — structural support load | The graph now uses a distance-to-opposite-support reaction ratio for two-support loads, which matches the simple 1D point-load beam ratio when actor origins adequately represent support/load points. Three-or-more supports still use a deterministic inverse-distance heuristic; this is not a general statics solver. |
| A09 — physical log cutting | Arbitrary `TrimLogToRange` requests no longer fake a cut by stretching a static mesh. `CutOffRemainder` now requires one valid authored split recipe and actual output mesh variants, validates output lengths against requested keep/offcut lengths, updates the resulting canonical geometry definitions and item world locations, and keeps source/offcut transforms aligned to the authored recipe. Unsupported ratios and missing meshes fail closed. General arbitrary-plane triangle slicing plus generated collision/mesh persistence is still not implemented; the known absent mesh family members remain unavailable. |
| A18/A19 — survival and crop climate | Weather cooling/warming, wind chill, wetness/drying and heat-thirst coefficients are now editable, range-checked parameters. Crop growth reads the shared survival weather/severity and interpolates configured weather growth multipliers. Balance, soil/season model depth and runtime behavior still need validation. |
| A20/A31 — load and rope movement | Zipline admission now checks nominal estimated item mass against an authored kilogram capacity. Along-rope acceleration uses gravity projected onto the slope minus a configurable Coulomb-like friction term from the normal component. Rope sag/tension/elasticity and multi-load dynamics remain outside this model. |
| A37 — operation-ledger retention | A generic TTL or oldest-first eviction is explicitly rejected because it could replay an old operation. Remaining capacity and completed-row counts are exposed; the bounded ledger logs and rejects new operations at capacity. Only a caller with a durable domain receipt/watermark may explicitly prune a completed operation (the raid pulse commits its watermark first). Other operation families still need their own durable receipt integration to avoid eventual fail-closed capacity exhaustion in long sessions. |
| A38 — carried-item save/load | Schema v19 adds carry owner identity/path, owner transform, carry component, held item identity/type and owner-relative item transform. Capture/preflight reconcile held item, physical actor and carry owner; load restores carrier transform, clears transient held/carry state, then reactivates the actor and rebuilds its carry/load relation. v17→v18 migration initializes an empty carry manifest; v18→v19 marks prior carry records as lacking an authoritative owner transform and uses the resolved current carrier rather than guessing. This has not been proven through Unreal pawn lifecycle, component registration or physics. |
| A06 — raid behavior | The raid adapter now supports authored `UWorldRaidTargetComponent` markers with priority/tie-break rules, a bounded configurable wave of primitive pawns, NavMesh path following, and damage pulses only after at least one raider reaches the target. The pulse remains fixed-step and ledger/watermark-backed. The pawn is still a placeholder without a behavior tree, combat animation or advanced enemy tactics; a valid NavMesh and an accessible target marker must be authored in the map. |

### Remaining boundaries after this follow-up

- No Unreal Engine 5.8 UHT/build, automated tests, Editor map open, PIE session or runtime test was run. All statuses above mean source code was added/changed and statically inspected, not engine-validated.
- A09 is closed only for exact authored split recipes whose meshes and layout validate. General arbitrary geometry slicing and generated collision/SaveGame persistence remain real feature work, not a test-only task.
- A03 still uses preflight plus compensating rollback rather than a single atomic swap across every subsystem. Engine actor destruction/BeginPlay/EndPlay cannot be fully reversed by a data-structure copy.
- A37 intentionally prioritizes idempotency safety over availability. If generic ledger capacity is reached before domain-specific durable receipts permit pruning, new world operations fail closed.
- The raid pawn requires a working NavMesh and a target marker that resolves to a reachable approach location. This code does not add or bake a NavMesh into any map.
- M001–M119 status records were not changed by this follow-up.
