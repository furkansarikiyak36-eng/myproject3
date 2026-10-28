# MyProject3 — Matematiksel ve Algoritmik Kod Denetimi

**Dal:** fix/vertical-slice-b-core-loop  
**Kapsam:** Üretim C++ kaynakları, ilişkili sistem mimarisi ve dikey kesit oluşturma betikleri.  
**Hariç:** Derleme, otomatik test çalıştırma, Unreal Editor/PIE ve runtime deneyi. Bu raporda bunlar kanıt olarak kullanılmadı.

## 1. Denetimin amacı

Bu rapor yalnızca kodun var olup olmadığını değil; algoritmaların matematiksel olarak tutarlı olup olmadığını, varsayımların açık olup olmadığını, farklı sistemlerin aynı kuralları kullanıp kullanmadığını ve bir prototipin henüz hangi anlamda yaklaşık kaldığını inceler. Bu statik inceleme runtime doğrulaması değildir. M001–M119 kayıtlarının durumları bu raporla değiştirilmez.

## 2. Özet: en önemli bulgular

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

### A03 — Save/load kısmi hata durumunda canlı dünyayı geri almıyor
**Dosya:** Source/MyProject3/WorldSaveCoordinatorSubsystem.cpp — ApplySnapshot; ApplyPhysicalActorSnapshots.

ApplySnapshot clock, item ledger, operation ledger, aktör transform/durumları, yapı grafiği, inventory, terrain, agriculture, survival, production, storage, cargo ve threat state'ini sırayla uygular. Daha sonraki bir RestoreSnapshot başarısız olursa önceki alt sistemlerdeki değişiklikleri geri almak için genel transaction yok. Fiziksel aktörler de sırayla değiştirildiği için ortada hata yarı yüklenmiş dünya bırakabilir.

**Gerekli ilke:** önce yan etkisiz tam doğrulama, sonra staged commit veya önceki durumun tam rollback'i. Başarısız load aktif dünyayı kısmen değiştirmemeli.

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

