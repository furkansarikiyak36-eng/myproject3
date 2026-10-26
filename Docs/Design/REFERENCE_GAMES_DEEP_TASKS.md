# MyProject3 — 9 Referans Oyunun Derin Mekanik Analizi ve Görev Listesi

Tarih: 26 Ekim 2026
Kaynak taban: `origin/demo/vertical-slice-phase-01` (d1ebd78). Kod referansları bu daldandır.
Durum: Bu bir tasarım/görev belgesidir. Hiçbir görev "yapıldı" sayılmaz; bitiş yalnızca **Kabul** satırı UE 5.8 içinde gösterildiğinde olur.

## 0. Okuma kuralları

- **Amaç kopya değil çekirdek:** Her oyundan, MyProject3'ün fiziksel-survival-inşa kimliğine uyan mekanik çekirdekler alınır. Her bölümün sonunda "Alınmayacaklar" listesi vardır.
- **Görev biçimi:** `ID — Ne yapılacak.` ardından *Kabul:* (gözlemlenebilir sonuç + en az bir negatif durum) ve gerektiğinde *Mevcut:* (projede bugün karşılığı olan kod) ve *Bağımlı:* (önce bitmesi gereken görev).
- **Öncelik:** P0 = dikey dilim için zorunlu çekirdek, P1 = oynanış derinliği, P2 = cila/genişleme.
- **Sayılar:** Oyunlardan aktarılan sayılar yalnızca bilinen ve sabit olanlar için verilir; diğer tüm değerler veri tablosunda ayarlanabilir başlangıç değeridir, sabit kod değeri değildir.
- **Ortak sistem kuralı:** Aynı kavram (ağırlık, ısı, yakıt, dayanıklılık, kalite, destek) tek bir sistemde yaşar. Bölüm 10'da hangi oyunun hangi ortak sistemi beslediği tablolanmıştır; oyuna özel ikinci bir sayaç açılmaz.

---

## 1. Sons of the Forest — fiziksel kaynak, serbest inşa, yardımcı AI

### 1.1 Mekanik ayrıştırması

SOTF'nin inşa sistemi tek bir "build menüsü" değildir; dört katmandan oluşur:

1. **Kaynak fiziği:** Ağaç baltayla kesilir, devrilir, gövde kütüklere ayrılır, kütük taşınır (aynı anda sınırlı adet, omuzda), kızakla toplu taşınır, depolama aktörlerine (kütük/çubuk/taş tutucuları) konur.
2. **Kütük işleme:** Yerleştirilmiş veya yerde duran kütüğe vurulan noktaya göre kesim yapılır; tam → yarım → çeyrek uzunluk, uzunlamasına bölme ile yarım kesit/plank. Kesim, kütüğün o anki durumuna ve vuruş noktasına bağlıdır.
3. **Serbest inşa (free-form):** Kütükler yatay duvar, dikey direk, eğik çatı/kiriş, zemin, merdiven olarak yerleştirilir; sistem yakın parçalara yaslanmayı ve boşluk tamamlamayı yönetir; yerleştirilmiş duvara vurularak pencere/kapı açılır. Ayrıca rehber kitaptan hazır yapı planları (blueprint) "hayalet" olarak yerleştirilir ve malzeme taşınarak tamamlanır.
4. **Yapı yaşamı:** Destek kaybedilince düşme, düşmanların yapılara saldırması, yardımcı karakterin (Kelvin) toplama/inşa tamamlama/alan temizleme komutları.

### 1.2 Kütük varyant matrisi — projedeki gerçek durum

Projede `ELogPieceType` 14 değer, `ELogCutAction` 7 eylem, `AssetPackages/UnrealExport` altında 58 FBX var. Eşleşme tablosu:

| Varyant | Nasıl oluşur | Enum karşılığı | FBX | Açık |
|---|---|---|---|---|
| Tam kütük | Ağaçtan | `WholeLog` / `FullLength+FullRound` | `SM_RawLog_600`, `Long_01..08`, `Short_01..04` | `Long_*`/`Short_*` ailelerinin hangi uzunluk/çapı temsil ettiği tanımsız |
| 3/4 uzunluk | Enine kesim | `ThreeQuarterLog` / `ThreeQuarterLength` | **Yok** (450 cm mesh yok) | Enum var, mesh yok → işlem fail-closed reddedilmeli ya da mesh üretilmeli |
| 1/2 uzunluk | Enine kesim | `HalfLengthLog` | `SM_RawLog_300` | — |
| 1/4 uzunluk | Enine kesim | `QuarterLog` | `SM_RawLog_150` | — |
| 75 cm parça | Enine kesim | **Enum yok** | `SM_RawLog_75_*` (9 dosya) | Mesh var, durum yok → `EighthLength` eklenmeli ya da 75 ailesi kaldırılmalı |
| Yarım kesit | Boyuna yarma | `SplitHalfLog` / `HalfRound` | `*_Half` (4 uzunluk) | — |
| Çeyrek kesit | Yarım kesiti yarma | `QuarterSectionLog` / `QuarterRound` | `*_Quarter_04` | `_04` son ekinin anlamı belgelenmemiş |
| Yontulmuş (tek yüz) | Balta/keser ile | `HewnLog` / `HewnFlat` | `*_Hewn1` | Enum tek değer, FBX iki ayrı durum |
| Yontulmuş (iki yüz) | İkinci yüz | **Yok** | `*_Hewn2` | `HewnTwoFaces` gerekli |
| Kalas / kare kiriş | 4 yüz yontma | `SquaredBeam` / `SquareBeam` | `*_Kalas` | Hewn2 → Kalas geçişi tanımlı değil |
| Plank | Kalas veya yarımdan dilimleme | `Plank` | `*_Plank` | Bir kütükten kaç plank çıktığı tanımsız |
| Oluk (gutter) | Yarım kesite oluk | `GutterLog` / `Grooved` | `*_Gutter` | Girdi koşulu (yalnızca yarım kesit) testsiz |
| Yalak (trough) | Tam/yarım kütüğü oyma | `TroughLog` / `Trough` | `*_Trough` | Su tutma işlevi yok (tarım/hayvan bağlantısı) |
| Çentikli | Uç çentik | `NotchedLog` / `Notched` | yalnız `SM_RawLog_75_FlatNotch_TopBottom` | Çentik tipleri ve diğer uzunluklar yok |
| Birleşim parçası | Zıvana vb. | `JointPiece` / `Joint` | **Yok** | Tamamen eksik |
| Sivri uç (kazık) | Ucu yontma | **Yok** | **Yok** | Savunma duvarı ve tuzak için zorunlu |
| Pencere/kapı kesikli duvar kütüğü | Yerleşik duvara vurma | **Yok** | **Yok** | Serbest inşanın temel parçası |
| Eğik uç (çatı/miter) | Açılı kesim | **Yok** | **Yok** | Çatı ve köşe birleşimi için |
| Yakacak odun (takoz) | Kısa parçayı yarma | **Yok** | **Yok** | Isı/yakıt sistemiyle tek bağlantı noktası |
| Kütük kalıntısı ve dallar | Ağaç kesimi | **Yok** | **Yok** | Dallar → çubuk kaynağı; kütük kalıntısı → arazi/temizlik |

Ek varyant **eksenleri** (her parçaya ortogonal uygulanır, yeni mesh gerektirmez):

- **Çap bandı:** ince, orta, kalın. Taşıma ağırlığını, destek kapasitesini ve plank sayısını etkiler.
- **Nem:** yaş, kuru. Ağırlığı, yanma değerini ve kurutma süresini etkiler.
- **Durum:** sağlam, hasarlı, yanık. Yapı dayanıklılığını belirler.
- **Tür (P2):** çam, meşe, huş gibi. Yoğunluk, dayanım ve yakıt değeri farkı. SOTF'de böyle bir ayrım yoktur; gerçekçilik katmanı olarak isteğe bağlıdır.

### 1.3 Görevler — ağaç ve hasat

- [ ] **SOTF-T01 (P0)** — Ağaç türü/boy/çap verisini `HarvestableTreeActor` için data asset'e taşı: gövde uzunluğu, çap bandı, gerekli vuruş sayısı, devrilme yönü kuralı, çıkacak kütük dizisi (ör. 600 + 300 + dallar). *Kabul:* iki farklı ağaç aynı kodla farklı kütük dizisi üretir; veri eksikse ağaç kesilebilir ama kütük üretmez ve hata loglar. *Mevcut:* `AHarvestableTreeActor::SpawnHarvestLogs`.
- [ ] **SOTF-T02 (P0)** — Vuruş yönüne göre devrilme yönü ve devrilme alanı kontrolü: devrilme yolunda yapı varsa yapıya `ApplyDamage` uygula. *Kabul:* evin yanındaki ağaç eve doğru devrilince duvar hasar alır; boş alanda hasar olayı yok.
- [ ] **SOTF-T03 (P0)** — Kütük kalıntısı (stump) aktörü: ağaç kesilince kalır, kalıcı kimliği vardır, inşa alanında yerleşimi engeller, kazma/sökme ile kaldırılabilir. *Kabul:* save/load sonrası kalıntı yerinde; kalıntı üstüne temel konamaz.
- [ ] **SOTF-T04 (P1)** — Dallar: devrilen ağaçtan belirli sayıda çubuk düşmesi ya da dal toplama eylemi; çubuklar `StickPrimitivePiece` olarak doğar ve sahiplik defterine kaydolur. *Kabul:* çubuk sayısı ağaç verisinden gelir; aynı ağaçtan ikinci kez çubuk düşmez.
- [ ] **SOTF-T05 (P1)** — Ağaç yeniden büyüme politikası: kapalı / zamanla fidan → ağaç. Shared clock'a bağlı, kayıtta tutulur. *Kabul:* açıkken N gün sonra kalıntı yerinde fidan; kapalıyken hiç büyümez.
- [ ] **SOTF-T06 (P2)** — Kesilen ağaç sayısının bölgesel "orman yoğunluğu" değerini düşürmesi; tehdit/keşif sistemleri bunu okuyabilir (açık alan = görünürlük).

### 1.4 Görevler — kütük varyantları ve işleme

- [ ] **SOTF-L01 (P0)** — #24/#25 kararından sonra kanonik durum: `{LengthCm, CrossSection, FaceCount, EndShape, Notches[], Openings[], Moisture, Condition}`. `ELogPieceType` yalnız eski kayıtları okumak için kalsın. *Kabul:* tek bir `ParseLegacy()` noktası; eski kayıt yeni duruma dönüşür; bilinmeyen değer yüklemeyi fail-closed durdurur.
- [ ] **SOTF-L02 (P0)** — 75 cm ve 3/4 tutarsızlığını kapat: ya `EighthLength` ekle ya 75 ailesini kaldır; 3/4 için ya 450 mesh üret ya da 3/4 işlemini kaldır. *Kabul:* her enum değerinin en az bir mesh satırı var, her mesh'in bir enum karşılığı var (otomatik doğrulama scripti).
- [ ] **SOTF-L03 (P0)** — `Hewn1`/`Hewn2`/`Kalas` zincirini eylem grafiği olarak tanımla: Round → Hewn(1 yüz) → Hewn(2 yüz) → Kalas(4 yüz) → Plank×N. Ara adım atlanırsa reddet. *Kabul:* tam yuvarlak kütüğe doğrudan Kalas reddedilir, hiçbir şey değişmez.
- [ ] **SOTF-L04 (P0)** — Vuruş noktasına göre enine kesim: kütük üzerinde vurulan konum (0–1 uzunluk oranı) en yakın izinli kesim noktasına yuvarlanır; çıkan iki parçanın uzunlukları toplamı kaynak uzunluğa eşittir. *Kabul:* 600'ü %50'den kesmek 300+300 verir; %25'ten kesmek 150+450 verir (450 mesh yoksa red).
- [ ] **SOTF-L05 (P0)** — Kesim ilerleme durumu: her vuruş `CutProgress` artırır; görsel olarak çentik/yarık decal'ı gösterilir; eşik dolmadan geometri değişmez; kayıt sırasında ilerleme kaydedilir. *Kabul:* 3 vuruşluk kesimin 2. vuruşunda save/load sonrası ilerleme aynı yerde, kütük hâlâ tek parça.
- [ ] **SOTF-L06 (P0)** — Sivri uç (`EndShape=Pointed`) eylemi: yalnızca yerde duran veya dikili kütükte, tek uçta; savunma duvarı ve tuzak kazığı girdisi. Mesh yoksa red. *Kabul:* sivri uçlu 300 kütük dikildiğinde `ATTACK` hasarı veren savunma parçası sayılır.
- [ ] **SOTF-L07 (P1)** — Yakacak odun: ≤150 cm parçayı yarma → N takoz; takoz bir item tanımıdır, yakıt değeri nem ve tür eksenlerinden gelir. *Kabul:* yaş takoz kuru takozdan daha az ısı verir (SURV-ortak yakıt sistemi üzerinden).
- [ ] **SOTF-L08 (P1)** — Çentik tipleri: üstten/alttan düz çentik, eyer (saddle) çentik, uç bindirme (lap). Çentikli kütükler köşe birleşiminde snap uygunluğunu belirler. *Kabul:* iki eyer çentikli kütük 90° köşede kilitlenir; çentiksiz kütük köşede yalnızca "yaslanır" (zayıf bağlantı).
- [ ] **SOTF-L09 (P1)** — Açıklık (opening) kesimi: duvara yerleşmiş kütüğe vurulan bölgede pencere/kapı açıklığı; açıklık genişliği komşu kütüklere yayılır; graph'ta kütük segmentlere bölünür. *Kabul:* 4 sıra duvarda kapı açınca üst sıra kütükler hâlâ iki yandan destekli; destek yoksa düşer.
- [ ] **SOTF-L10 (P1)** — Eğik uç kesimi (çatı/miter, 30/45/60°): çatı kirişleri ve köşe birleşimi için. *Kabul:* 45° uçlu iki kütük çatı tepesinde birleşir ve tek düğüm çifti olarak graph'a girer.
- [ ] **SOTF-L11 (P1)** — Plank verimi: kalastan veya yarım kesitten çıkan plank sayısı çap bandına göre tabloda (ör. ince 2, orta 3, kalın 4); toplam hacim korunumu ± fire. *Kabul:* plank sayısı tabloyla birebir; `ITEM-06` hacim dengesi testi geçer.
- [ ] **SOTF-L12 (P1)** — Nem ekseni: yeni kesilmiş kütük yaş; açıkta/raf altında shared clock ile kurur; yağmur nemi artırır. Ağırlık = hacim × yoğunluk × nem katsayısı. *Kabul:* aynı kütük kuruyunca taşıma ağırlığı düşer ve kayıt sonrası korunur.
- [ ] **SOTF-L13 (P2)** — Yanık/hasarlı durum: yangın veya baskın hasarından sonra kütük dayanımı düşer, mesh'e malzeme parametresi ile yansır; onarılmaz, değiştirilir.
- [ ] **SOTF-L14 (P0)** — FBX manifest scripti (`UnrealEditorScripts` altına): her mesh için bounds, ana eksen, pivot konumu, gerçek uzunluk (cm) ve hangi kanonik duruma bağlandığı. *Kabul:* manifest dosya adındaki sayı ile ölçülen uzunluk %2'den fazla saparsa script hata verir.

### 1.5 Görevler — taşıma ve toplu lojistik

- [ ] **SOTF-C01 (P0)** — Taşıma yuvaları: omuzda en fazla 2 kütük (veri tablosundan), elde 1 taş veya demet çubuk; ağırlık toplamı hareket hızını ve stamina tüketimini etkiler. *Mevcut:* `ULogCarryComponent`. *Kabul:* 3. kütük alma reddedilir, hiçbir şey değişmez; 2 kütükle koşu hızı veri değerine düşer.
- [ ] **SOTF-C02 (P0)** — Fırlatma/bırakma ayrımı: bırakılan kütük fizikle düşer ve sahipliği `World` olur; fırlatılan kütük ivme alır ve çarptığı nesneye hasar uygular. *Kabul:* fırlatılan kütük yapıya çarparsa `ApplyDamage` bir kez çağrılır.
- [ ] **SOTF-C03 (P1)** — Kütük kızağı (sled): fiziksel itilen aktör; içinde N kütük/taş/çubuk yuvası; yüklü kızak eğimde yavaşlar (arazi eğim maliyeti, DS-ortak). Kızak içindeki item'lar `InTransit` sahipliğinde. *Kabul:* kızak devrilirse yük dünyaya dökülür ve her item tek sahipli kalır.
- [ ] **SOTF-C04 (P1)** — Depolama tutucuları: kütük tutucu, çubuk tutucu, taş tutucu — her biri yalnız kendi item kategorisini kabul eder, görsel olarak dolar (instanced mesh), kapasite veri tablosunda. *Mevcut:* `WorldStorageSubsystem`, `WorldStorageComponent`. *Kabul:* taş tutucuya kütük konulamaz; doluluk görsel sayısı authoritative stokla aynı.
- [ ] **SOTF-C05 (P2)** — Kızak/tutucudan doğrudan inşa: blueprint hayaletine yakın tutucudaki malzeme otomatik tüketilir (mesafe sınırı ile).

### 1.6 Görevler — serbest inşa

- [ ] **SOTF-B01 (P0)** — Yerleştirme modları: yatay (duvar/zemin), dikey (direk), çapraz (kiriş/çatı). Her mod için zemin/destek kuralı ayrı. *Mevcut:* `ELogPlacementMode`. *Kabul:* dikey direk ancak zemin/temel/terrain anchor üstüne konabilir; havada reddedilir.
- [ ] **SOTF-B02 (P0)** — Duvar sırası (course) mantığı: ilk sıra zemine, sonraki sıralar alttaki sıranın üstüne oturur; uç uca eklenen kütükler aynı sıra sayılır. *Kabul:* 3 sıra duvar tek "wall assembly" kimliği altında graph'ta zincirlenir; ortadaki kütük kesilirse üst sıra yalnız kalan desteklerle değerlendirilir.
- [ ] **SOTF-B03 (P0)** — Preview = commit: hayalet önizleme ve gerçek yerleştirme aynı hesaplayıcıdan; geçersiz durumda hayalet kırmızı ve sebep kodu gösterir (ör. "destek yok", "çakışma", "çentik uyumsuz"). *Kabul:* her red sebebi için ayrı UI metni; önizleme yeşilken commit asla reddedilmez (aynı frame içinde).
- [ ] **SOTF-B04 (P1)** — Zemin döşeme: kirişler arası yatay kütük/plank döşemesi; döşeme ancak iki uçtan kirişe oturursa geçerli; açıklık (span) uzunluğu çap bandına bağlı limit. *Kabul:* limiti aşan açıklıkta zemin reddedilir.
- [ ] **SOTF-B05 (P1)** — Çatı: eğik kirişler + plank/kütük kaplama; çatı altında kalan alan "barınak" bölgesi olarak işaretlenir (SURV barınak sistemine girdi). *Kabul:* çatısız 4 duvar barınak sayılmaz; çatılı oda yağmurda ıslanmayı durdurur.
- [ ] **SOTF-B06 (P1)** — Merdiven ve rampa: kütük/plank basamaklı merdiven; eğim limiti; karakter hareketi ve navmesh ile çalışır. *Kabul:* merdiven ile ikinci kata çıkılır; AI aynı merdiveni rota olarak kullanır.
- [ ] **SOTF-B07 (P1)** — Kapı ve kapak aktörü: açıklığa takılır, açılır/kapanır, kilitlenir; düşman için kırılabilir hedef. *Kabul:* kapı kapalıyken AI rota maliyeti yükselir; kırılınca maliyet düşer.
- [ ] **SOTF-B08 (P1)** — Hazır yapı planı (blueprint hayaleti): plan seçilir, hayalet yerleştirilir, gerekli malzeme listesi gösterilir, her malzeme konuldukça ilgili parça "gerçekleşir". *Kabul:* yarım plan save/load sonrası aynı ilerlemede; plan iptal edilince konmuş malzeme geri verilir (çift değil).
- [ ] **SOTF-B09 (P2)** — Mobilya ve işlevsel parçalar: raf, masa, yatak (SURV-uyku), kurutma rafı (SURV-yemek), yağmur toplayıcı (SURV-su).

### 1.7 Görevler — taş ve çubuk inşa

- [ ] **SOTF-S01 (P0)** — Taş yerleştirme: taşlar zemin, yol ve duvar olarak dizilir; taş yolu yürüyüş/kızak maliyetini düşürür (LOG ortak). *Mevcut:* `AStoneBuildingPiece`, `EStonePieceType`. *Kabul:* taş yol döşenen edge'in rota maliyeti düşer.
- [ ] **SOTF-S02 (P1)** — Taş + kütük karma yapı: taş temel üstüne kütük duvar; taş şömine/ocak (ateş kaynağı + barınak ısısı). *Kabul:* taş ocaklı oda ısı bölgesi olur.
- [ ] **SOTF-S03 (P0)** — Çubuk yapılar: çit, sivri çubuk savunma hattı, çubuk tutucu, çubuk raf. *Mevcut:* `AStickPrimitivePiece`, `EStickPrimitiveState`. *Kabul:* sivri çubuk çiti temas eden düşmana hasar verir ve kendisi dayanıklılık kaybeder.
- [ ] **SOTF-S04 (P1)** — Halat: bez/lif ile üretilen item; halatla bağlanan çubuk/kütük birleşimleri daha güçlü bağlantı kategorisi alır; halat köprü/merdiven (DS ortak).

### 1.8 Görevler — destek, çökme ve hasar

- [ ] **SOTF-D01 (P0)** — Çoklu destek: bir parçanın birden fazla destekleyicisi olabilir (zemin iki kirişe oturur). Mevcut tek parent modeli (`ParentAnchorId`) çoklu edge'e genişletilmeli. *Kabul:* iki kirişten biri kaldırılınca zemin ayakta; ikisi kaldırılınca düşer.
- [ ] **SOTF-D02 (P0)** — Çökme fiziği: destek kaybeden parça "düşen fizik nesnesi"ne dönüşür, graph'tan çıkar, sahipliği `World` olur; çarptığı parçaya hasar. *Kabul:* çöken çatı kütükleri yerde toplanabilir item olarak kalır; graph'ta yetim düğüm yok.
- [ ] **SOTF-D03 (P1)** — Söküm: oyuncu yerleştirilmiş parçayı söker; parça item olarak geri gelir (durumuna göre); bağımlı parçalar önce kontrol edilir. *Kabul:* üstünde yük olan kütük sökülmek istenince ya red ya da bağımlı çökme önizlemesi.

### 1.9 Görevler — yardımcı AI (Kelvin benzeri)

- [ ] **SOTF-A01 (P1)** — Komut sistemi: "kütük topla", "çubuk topla", "taş topla", "yapıyı tamamla", "alanı temizle". Her komut hedef bölge + hedef depo ile verilir. *Kabul:* yardımcı topladığı kütüğü en yakın uygun tutucuya koyar; tutucu doluysa durur ve sebebini bildirir.
- [ ] **SOTF-A02 (P1)** — Yardımcı aynı sahiplik API'sini kullanır (oyuncuya özel kısayol yok). *Kabul:* yardımcının taşıdığı kütük `Held(AgentId)` sahipliğinde; kayıt sırasında elde ise kurala göre yere bırakılır.
- [ ] **SOTF-A03 (P2)** — Yardımcının ihtiyaçları ve yaralanması (SURV profilini paylaşır).

### 1.10 Alınmayacaklar

Hikâye/mağara ilerleme kapıları, mutant tasarımı, GPS/izleyici ekipman zinciri, çok oyunculu senkronizasyon.

---

## 2. 7 Days to Die — blok inşa, yapısal bütünlük, kazı, baskın AI

### 2.1 Mekanik ayrıştırması

1. **Blok + şekil sistemi:** Her blok bir malzeme ve bir şekildir (tam küp, rampa, kama, levha, direk, köşe vb.); döndürme yönleri vardır. MyProject3 voxel-blok oyunu değildir; buradan alınacak olan **malzeme kademesi ve yükseltme/onarım** mantığıdır.
2. **Yükseltme zinciri:** çerçeve → ahşap → kaba taş → beton → çelik. Her kademe malzeme ve zaman ister, blok sağlığını ve kütlesini artırır.
3. **Yapısal bütünlük:** Her malzemenin kütlesi ve taşıyabileceği yük vardır; zeminden kopuk bağlantı yük sınırını aşınca bloklar zincirleme düşer.
4. **Pürüzsüz arazi ve madencilik:** Kazılabilir arazi blokları (toprak, çakıl, kum, taş), cevher damarları; alet ve beceriye göre verim.
5. **Kan Ayı baskını (horde):** Varsayılan her 7. gecede zamanlanmış dalga; zombiler oyuncuya giden **en düşük maliyetli yolu** seçer, blok sağlığını yolun maliyetine katar, zayıf bloğu kırar.
6. **Isı haritası (heat):** Gürültü, ateş, ocak, kazma gibi faaliyetler bölgesel ısı biriktirir; eşik aşılınca gözcü/çağırıcı düşman gelir.
7. **Tuzak ve güç:** Sivri tuzaklar, bıçaklı tuzaklar, elektrikli çit, taretler; jeneratör/akü/güneş paneli ile kablolu güç.
8. **Atölye ve kalite:** Tezgâh, ocak (eritme), çimento karıştırıcı, kimya istasyonu; üretim kuyruğu; eşya kalite kademeleri.
9. **Survival debuff'ları:** Kanama, kırık bacak, burkulma, enfeksiyon, ishal; her birinin özel tedavisi.

### 2.2 Görevler — malzeme kademesi, yükseltme, onarım

- [ ] **7D-M01 (P0)** — Yapı parçası "kademe" alanı: ör. çubuk çerçeve → kütük → yontulmuş kütük → taş kaplama. Her kademe: gerekli item listesi, iş süresi, sağlık, kütle. *Kabul:* çerçeve parçası yükseltilince sağlık ve kütle tablo değerine geçer; malzeme yoksa red, hiçbir şey değişmez.
- [ ] **7D-M02 (P0)** — Onarım eylemi: hasarlı parça kendi kademe malzemesiyle onarılır; onarım miktarı malzeme başına sabit. *Mevcut:* `WorldStructuralGraphSubsystem` `Damage`/`DamageThreshold`. *Kabul:* %40 hasarlı duvar 1 kütükle %X onarılır; tam sağlıkta onarım malzeme tüketmez.
- [ ] **7D-M03 (P1)** — Hasar görselliği: hasar yüzdesine göre 3 kademe (çatlak, kırık, yıkılmak üzere) malzeme parametresiyle. *Kabul:* görsel kademe authoritative hasarla birebir; kayıt sonrası korunur.

### 2.3 Görevler — yapısal bütünlük

- [ ] **7D-S01 (P0)** — Malzeme başına yük profili tablosu: kütle, maksimum taşıma, maksimum yatay uzanma (konsol). *Mevcut:* `LocalLoad`, `MaxSupportedLoad`. *Kabul:* tablo değiştirince aynı yapı farklı noktada çöker (sabit kod yok).
- [ ] **7D-S02 (P0)** — Toplam yük hesabı: her düğümün taşıdığı yük = kendi kütlesi + üstündekilerin payı (çoklu destekte paylaştırılarak). *Bağımlı:* SOTF-D01. *Kabul:* 3 katlı direkte en alttaki direğin yükü üstteki ikisinin toplamı + kendisi.
- [ ] **7D-S03 (P0)** — Zincirleme çökme sırası: aşırı yüklenen düğüm düşer → yük yeniden dağıtılır → yeni aşırı yükler aynı adımda değil sıradaki adımda değerlendirilir (deterministik). *Kabul:* aynı yapı aynı sırayla çöker (iki çalıştırmada aynı düğüm dizisi).
- [ ] **7D-S04 (P1)** — Hata ayıklama görünümü: her parçanın yük oranını (yeşil→kırmızı) gösteren debug modu. *Kabul:* oyuncu yapı yükünü görebilir; kırmızı parça bir sonraki ek parçada çöker.

### 2.4 Görevler — arazi kazısı ve madencilik

- [ ] **7D-T01 (P0)** — Kazılabilir malzeme sınıfları: toprak, çakıl, kum, kil, taş, cevher. Her biri sertlik, gereken alet, verim ve düşen item ile. *Mevcut:* `WorldTerrainStateSubsystem` `MaterialId`. *Kabul:* kürek taşı kazamaz (red); kazma taşı kazar ve taş item düşer.
- [ ] **7D-T02 (P1)** — Cevher damarı yerleşimi: seed + derinlik + bölgeye göre deterministik damar; yüzeyde iz (renk/kaya) bulunur. *Kabul:* aynı seed aynı damarları üretir.
- [ ] **7D-T03 (P1)** — Kazı tavan desteği: yer altı boşluğu üstünde destek yoksa tavan çöker (sınırlı alan); destek direği/kiriş eklenebilir. *Bağımlı:* TERR görevleri, STRUCT terrain-support. *Kabul:* destekli tünel ayakta; desteksiz geniş oda çöker.

### 2.5 Görevler — baskın AI

- [ ] **7D-R01 (P0)** — Baskın takvimi: her N günde bir (veri), uyarı süresi, dalga sayısı, dalga başı bütçe; gün ilerledikçe artan bütçe. *Mevcut:* `WorldThreatDirectorSubsystem::InitializeDirector(FirstRaidDelaySeconds)`. *Kabul:* 7. gün gecesinde baskın başlar; aynı tick yeniden işlenirse ikinci baskın açılmaz.
- [ ] **7D-R02 (P0)** — Yol maliyeti = mesafe + kırılacak parçaların sağlığı × kırma katsayısı. AI hedefe "kırarak geçme" dahil en ucuz yolu seçer. *Kabul:* tek kapılı kalede AI kapıya yönelir; kapı demirden, yan duvar çürük kütükse duvara yönelir.
- [ ] **7D-R03 (P0)** — Kırma davranışı: AI yolu kapatan parçaya saldırır, hasar `ApplyDamage` üzerinden; parça düşünce yol yeniden hesaplanır. *Mevcut:* `WorldRaidAdapterSubsystem` Pawn spawn + periyodik hasar. *Kabul:* parça kırılınca AI 1 sn içinde yoluna devam eder.
- [ ] **7D-R04 (P1)** — Isı haritası: faaliyet türüne göre ısı puanı (ateş, kesim, kazı, üretim makinesi, silah), bölgesel ızgara, zamanla sönüm. Eşik → keşif düşmanı. *Kabul:* çok ocak çalışan üs ızgarasında ısı yükselir; faaliyet bitince sönümlenir; kayıtta tutulur.
- [ ] **7D-R05 (P1)** — Düşman türleri veri ile: yürüyen, koşan (gece), kazan (yer altından), tırmanan. İlk sürüm: yürüyen + kırıcı. *Kabul:* iki tür aynı kod yolunda farklı davranır.

### 2.6 Görevler — tuzak ve savunma

- [ ] **7D-D01 (P0)** — Sivri tuzak: temas hasarı, kendi dayanıklılığı, yerleştirme zemin kuralı. *Mevcut:* `EPrimitiveTrapState`. *Bağımlı:* SOTF-L06/S03.
- [ ] **7D-D02 (P1)** — Tetiklemeli tuzak (düşen kütük, ip tuzağı): kur → gergin → tetiklendi → yeniden kur. *Kabul:* tetiklenen tuzak yeniden kurulmadan ikinci kez çalışmaz.
- [ ] **7D-D03 (P2)** — Güç ağı ile çalışan savunma (Satisfactory/Hydroneer güç sistemi ortak). İlk sürümde kapsam dışı bırakılabilir.

### 2.7 Görevler — atölye, kalite, survival debuff

- [ ] **7D-W01 (P1)** — Eşya kalite kademesi (1–N): alet hasarı, verim ve dayanıklılığı etkiler; üretimde istasyon kademesi ve beceri kaliteyi belirler. *Kabul:* kalite 3 balta, kalite 1 baltadan daha az vuruşla ağaç keser (tablo).
- [ ] **7D-W02 (P1)** — Eritme ocağı: ham cevher → birim malzeme; yakıt tüketir; kuyruk. *Bağımlı:* PROD ortak, SURV yakıt ortak.
- [ ] **7D-H01 (P1)** — Yaralanma: kanama (zamanla can kaybı; bandajla durur), burkulma/kırık (hız düşer; atel ile). Yüksekten düşme ve baskın hasarı tetikler. *Bağımlı:* TLD-A görevleri ile aynı rahatsızlık sistemi.

### 2.8 Alınmayacaklar

Blok-voxel dünya, yüzlerce şekil menüsü, tüccar/görev ekonomisi, araçlar, okunan dergiyle tarif açma.

---

## 3. Death Stranding — kargo, arazi geçişi, oyuncu altyapısı

### 3.1 Mekanik ayrıştırması

1. **Kargo modeli:** Her kargonun ağırlığı, boyutu, kabın dayanıklılığı vardır; sırt, kollar, kalça ve kemer yuvalarına yerleştirilir. Yük dağılımı ağırlık merkezini değiştirir; oyuncu dengeyi aktif olarak korur, dengesiz yük düşmeye yol açar.
2. **Kargo durumu:** Düşme ve çarpma kabı hasarlar; zaman yağmuru kapları aşındırır; tamir spreyi kabı onarır; teslimatta süre, hasar ve ağırlık puanlanır.
3. **Arazi okuma:** Tarayıcı araziyi zorluk renklerine boyar (dik eğim, derin/akıntılı su, kaygan zemin); oyuncu harita üzerinde rota çizer.
4. **Geçiş ekipmanı:** Taşınabilir merdiven (yatay köprü veya dikey tırmanış), tırmanma çapası + halat, sonra kalıcı yapılar: köprü, jeneratör, posta kutusu, gözetleme kulesi, sığınak, zipline.
5. **Yol inşası:** Otoyol segmentleri, makineye malzeme teslim edilerek onarılır; yol araçlar için yüksek hız sağlar.
6. **Bozulma:** Kalıcı yapılar zamanla aşınır; bakım için malzeme eklenir.
7. **Tehdit ve hırsızlık:** Hırsız gruplar kargoyu algılayıp çalar; kargo geri alınabilir.
8. **Asenkron paylaşım:** Diğer oyuncuların yapıları dünyada görünür. (MyProject3 için kapsam dışı.)

### 3.2 Görevler — kargo ve denge

- [ ] **DS-C01 (P0)** — Item tanımına `MassKg`, `Volume`, `CarrySlotType` (sırt, el, omuz, kalça) ekle; bilinmeyen kütle = red. *Bağımlı:* ITEM-01. *Kabul:* tanımsız kütleli item alınamaz.
- [ ] **DS-C02 (P0)** — Taşıma kapasitesi: yuva sayısı + toplam kütle sınırı; sınırı aşınca "aşırı yük" durumu (hız ve stamina cezası) ve üst sınırda red. *Kabul:* sınırın %100'ünde yürüme yavaşlar; %130'unda alma reddedilir (tablo).
- [ ] **DS-C03 (P1)** — Ağırlık merkezi ve denge: yükün konumu karakterin denge değerini etkiler; eğim + koşu + dengesiz yük = sendeleme; sendeleme düzeltilmezse düşme ve yükün dökülmesi. *Kabul:* tek omuzda 2 kütük ile dik inişte sendeleme olasılığı veri tablosuna göre artar; düşünce kütükler dünyaya dökülür, her biri tek sahipli.
- [ ] **DS-C04 (P1)** — Kap/ambalaj dayanıklılığı: hassas kargonun (ör. tohum, yiyecek, cam) kabı düşmede hasar alır; kap %0 olunca içerik hasar alır. *Kabul:* düşmede kap hasarı yükseklikle orantılı.
- [ ] **DS-C05 (P2)** — Teslimat değerlendirmesi: hedefe süre/hasar/kütleye göre puan; köylü/yerleşim memnuniyetine bağlanabilir.

### 3.3 Görevler — arazi geçiş maliyeti

- [ ] **DS-T01 (P0)** — `RouteCostProvider`: kenar maliyeti = mesafe × (eğim katsayısı) × (yüzey katsayısı: çamur, kar, taş yol, kum) × (yük katsayısı) × (hava katsayısı). *Mevcut:* `FLogisticsRouteEdge{FromNode, ToNode, Cost, bEnabled}` (sabit `Cost`). *Kabul:* aynı kenarda yağmurda çamur maliyeti açık havadakinden yüksek.
- [ ] **DS-T02 (P1)** — Arazi tarayıcı görünümü: oyuncu etrafında eğim ve su derinliğini renklerle gösteren tarama. *Kabul:* eğim > yürünebilir limit kırmızı; derin su ayrı renk.
- [ ] **DS-T03 (P1)** — Su geçişi: sığ su yavaşlatır, orta su dengeyi zorlar, derin su yükü sürükler (yük dökülür). *Kabul:* derin suda düşen kütük akıntı yönünde sürüklenir (fizik) ve sahipliği `World` olur.
- [ ] **DS-T04 (P2)** — Ayakkabı aşınması: yürünen mesafe ve zemine göre ekipman dayanıklılığı düşer (TLD giysi sistemi ortak).

### 3.4 Görevler — geçiş ekipmanı ve kalıcı altyapı

- [ ] **DS-I01 (P0)** — Taşınabilir merdiven (kütük/çubuk + halat ile üretilir): iki uçtan zemine oturursa köprü, dikeyde tırmanış. Yerleştirilince rota grafiğine kenar ekler. *Kabul:* derenin üstüne merdiven konunca rota uzunluğu kısalır; merdiven alınınca kenar silinir.
- [ ] **DS-I02 (P1)** — Halat ve çapa: yamaca çapa çakılır, halat inişi sağlar; çapa zemin türüne göre tutar/tutmaz. *Kabul:* gevşek kumda çapa reddedilir.
- [ ] **DS-I03 (P1)** — Kütük köprü: iki yakaya oturan kütükler + plank döşeme; açıklık limiti çap bandına bağlı; graph'ta iki terrain anchor arasında. *Bağımlı:* SOTF-B04, STRUCT çoklu destek. *Kabul:* bir ucu oyulan (kazı) köprü çöker ve rota kenarı geçersizleşir.
- [ ] **DS-I04 (P1)** — Zipline: iki direk arası kablo; yalnız kargo (ve/veya oyuncu) taşır; kargo `InTransit`. *Kabul:* zipline'dan gönderilen kütük karşı uçta tek sahipli olarak iner; direk yıkılınca yoldaki kargo düşer.
- [ ] **DS-I05 (P1)** — Yol yapımı: taş/çakıl serilerek patika → yol; yol kenar maliyetini düşürür ve kızak/araba hızını artırır. *Kabul:* yol yapılan kenarda kızak süresi tabloya göre kısalır.
- [ ] **DS-I06 (P2)** — Aşınma ve bakım: kalıcı yapılar zamanla sağlık kaybeder (yağmur/kar hızlandırır); bakım malzemesi eklenir. *Kabul:* bakımsız köprü N günde kullanılamaz duruma gelir.

### 3.5 Görevler — kargo tehdidi

- [ ] **DS-R01 (P2)** — Hırsız NPC: gözetimsiz depo veya bırakılmış kargoyu hedefler, kendi kampına taşır; kargo kampta geri alınabilir. *Bağımlı:* RAID AI, ITEM sahiplik. *Kabul:* çalınan item hırsızın `Held` sahipliğinde; hırsız öldürülünce dünyaya düşer.

### 3.6 Alınmayacaklar

Asenkron oyuncu ağı, BT/timefall hikâye mantığı, bebek mekaniği, motorlu araç filosu (ilk sürüm).

---

## 4. Farming Simulator — tarla işleri, toprak, büyüme, hayvan, üretim

### 4.1 Mekanik ayrıştırması

1. **Tarla işlem döngüsü:** sürme veya kültivatör → (taş toplama) → ekim → gübreleme (katı, sıvı, gübre/şerbet, kireç) → yabancı ot mücadelesi → (merdane) → hasat → anız/malçlama. Her işlem tarla yüzeyinin bir "durum katmanı"nı değiştirir.
2. **Toprak durumu:** pH (kireç), azot seviyesi (gübre adımları), sürülmüş/sürülmemiş bonus, ekim nöbeti bonusu, sıkışma, yabancı ot evresi, taş yoğunluğu. Hassas tarımda (precision farming) toprak örneği ile bölgesel farklılık.
3. **Mevsimsel büyüme takvimi:** Her ürünün ekim ve hasat ayları; evre sayısı ürüne göre değişir; hasat zamanı kaçarsa ürün kurur.
4. **Hasat ve depolama:** Hasat ana ürün + yan ürün (saman) verir; ürün silo/ambara gider; balya (kare/yuvarlak), silaj sarma ve fermantasyon.
5. **Hava:** Islak ürün hasat edilemez (ayara bağlı); kar tarlayı kapatır; don.
6. **Hayvanlar:** Türe göre yem karışımı, su, altlık (saman), temizlik; çıktı (süt, yün, yumurta, gübre/şerbet), üreme, sağlık/verimlilik.
7. **Üretim zinciri:** Değirmen, fırın, mandıra, bıçkıhane; girdi/çıktı oranları.
8. **Makine ve ekipman:** Çalışma genişliği, yakıt, aşınma/bakım, alet bağlama.
9. **Ekonomi:** Fiyat dalgalanması, satış noktaları, sözleşmeler; yardımcı işçiler.
10. **Ormancılık:** Ağaç kesimi, istenen uzunlukta kesme, yonga, odun satışı. **Bu, SOTF kütük sistemiyle aynı çekirdektir.**

### 4.2 Görevler — tarla ve toprak

- [ ] **FS-F01 (P0)** — Tarla parseli kimliği: arazi üzerinde çokgen veya ızgara bölge; hücre çözünürlüğü (ör. 1 m²); her hücrede durum katmanları. *Mevcut:* `FWorldCropState` tekil ürün kaydı; parsel yok. *Kabul:* parsel save/load sonrası aynı sınır ve hücre durumlarıyla gelir.
- [ ] **FS-F02 (P0)** — Hücre durum katmanları: `Tilled`, `Moisture`, `Nitrogen`, `pH`, `Weeds`, `Stones`, `CropId`, `GrowthStage`, `Compaction`. *Kabul:* her katman ayrı API ile değişir ve ayrı revizyonu yoktur (hücre revizyonu ortak).
- [ ] **FS-F03 (P0)** — El aleti ile sürme (çapa/kazma): hücreyi `Tilled` yapar; taşlı hücrede taş çıkar (taş item, SOTF taş sistemi). *Kabul:* sürülmemiş hücreye ekim reddedilir; taş toplanınca `Stones` azalır.
- [ ] **FS-F04 (P0)** — Ekim: tohum item tüketir, hücre başına ekim yoğunluğu; yanlış mevsimde ekim izinli ama büyüme yok (ürün tablosu). *Kabul:* 10 hücreye ekim 10 tohum tüketir; tohum azsa kalan kadar ekilir ya da tamamı reddedilir (kural seçilip testlenir).
- [ ] **FS-F05 (P1)** — Gübreleme: gübre item'ı (hayvan gübresi, kompost, kül) azotu bir kademe artırır; kademeler verim çarpanı. *Kabul:* 0 kademe %X, 2 kademe %Y verim (tablo).
- [ ] **FS-F06 (P1)** — Kireç/kül ile pH düzeltme: pH aralık dışında verim cezası. *Kabul:* düşük pH tarlada aynı ürün daha az verir.
- [ ] **FS-F07 (P1)** — Yabancı ot: zamanla büyür, ürünle rekabet eder; çapa ile temizlenir. *Kabul:* temizlenmeyen tarlada verim cezası.
- [ ] **FS-F08 (P1)** — Sulama: yağmur nemi artırır (SURV hava ortak), güneş/sıcak azaltır; kova/su kabı ile elle sulama (su item'ı, yalak ile ilişki). *Kabul:* 3 gün kuraklıkta ürün `Withered` riskine girer. *Mevcut:* `SetCropMoisture`.
- [ ] **FS-F09 (P2)** — Ekim nöbeti: aynı hücreye üst üste aynı ürün verim cezası; baklagil sonrası bonus.

### 4.3 Görevler — ürün ve büyüme

- [ ] **FS-G01 (P0)** — Ürün tanımı data asset: evre sayısı ve süreleri, ekim ayları, hasat penceresi, su ve azot ihtiyacı, don toleransı, ana ürün ve yan ürün verimi, tohum geri dönüş oranı. *Mevcut:* `ECropGrowthStage {Seed, Sprout, Growing, Mature, Withered}` sabit 5 evre. *Kabul:* evre sayısı ürün tanımından gelir; sabit enum yalnız genel kategori.
- [ ] **FS-G02 (P0)** — Mevsim takvimi: shared clock üzerinden ay/mevsim; ürün büyümesi yalnız izinli aylarda. *Bağımlı:* CLOCK-01. *Kabul:* kış ayında ekilmiş buğday büyümez; ilkbaharda başlar.
- [ ] **FS-G03 (P0)** — Hasat: olgun hücre → ana ürün item (ağırlık/hacim ile) + yan ürün (saman); hasat sonrası hücre `Stubble`. *Kabul:* aynı hücre ikinci kez hasat edilemez; aynı OperationId tekrarında ürün çoğalmaz.
- [ ] **FS-G04 (P1)** — Hasat penceresi kaçarsa ürün kalite/verim düşer, sonra kurur. *Kabul:* pencere sonrası N gün hasat %Y verim.
- [ ] **FS-G05 (P1)** — Don ve kar: don toleransı düşük ürün donda hasar alır; karla örtülü tarla işlenemez. *Bağımlı:* SURV hava.
- [ ] **FS-G06 (P1)** — Islak hasat kuralı: yağmur sonrası belirli süre hasat edilen tahıl "ıslak" — kurutulmadan depolanırsa çürür. *Kabul:* ıslak tahıl kurutma rafı/ambarda kurur; ıslak depolanan tahıl bozulma hızı yüksek.
- [ ] **FS-G07 (P1)** — İlk ürün seti (dar kapsam): 1 tahıl (buğday: un → ekmek zinciri), 1 kök (patates: doğrudan yemek), 1 lif (keten: halat/bez zinciri — SOTF halatına bağlanır).

### 4.4 Görevler — depolama ve işleme

- [ ] **FS-P01 (P1)** — Ambar/silo: dökme ürünü miktar olarak saklar (tekil aktör değil, stok); kapasite ve nem. *Mevcut:* `WorldStorageSubsystem`. *Kabul:* 1000 birim buğday tek stok kaydı; içeri/dışarı akış sahiplik defterinde.
- [ ] **FS-P02 (P1)** — Saman balyası: el ile demet/balya; altlık (hayvan) ve çatı kaplaması (barınak) girdisi. *Kabul:* saman çatı plank çatıdan daha az dayanıklı ama ucuz.
- [ ] **FS-P03 (P1)** — Değirmen (el değirmeni → su değirmeni): tahıl → un; su değirmeni Hydroneer su/güç sistemine bağlanır. *Bağımlı:* PROD.
- [ ] **FS-P04 (P2)** — Silaj/fermantasyon: kapalı kapta zamanla yem dönüşümü.

### 4.5 Görevler — hayvanlar

- [ ] **FS-A01 (P0)** — İlk hayvan türü (tavuk veya keçi): ağıl/kümes kapasitesi, yem + su + altlık ihtiyacı, çıktı (yumurta veya süt), gübre. *Mevcut:* `FWorldAnimalState`, `FeedAnimal(AnimalId, HungerReduction, ExpectedRevision)` — item tüketmiyor. *Kabul:* yem item'ı yemliğe konur; hayvan yemlikten tüketir; yemlik boşsa açlık artar.
- [ ] **FS-A02 (P0)** — Yemlik ve suluk aktörü: depolama bileşeni olan fiziksel aktör; yalak (`TroughLog`) suluk olarak kullanılabilir. *Kabul:* kütük yalak suluk işlevi görür (kütük sistemiyle tek bağlantı).
- [ ] **FS-A03 (P0)** — Ürün üretimi item olarak: yumurta/süt sayaç değil, sahiplik defterinde doğan item; toplama eylemi. *Kabul:* toplanmayan yumurta N gün sonra bozulur (TLD bozulma ortak).
- [ ] **FS-A04 (P1)** — Temizlik ve sağlık: altlık değişmezse temizlik düşer, sağlık ve verim düşer. *Kabul:* temizlik %30 altında çıktı %50.
- [ ] **FS-A05 (P1)** — Üreme: koşullar sağlanırsa N günde yavru; kapasite doluysa üreme durur.
- [ ] **FS-A06 (P1)** — Hayvan fiziksel davranışı: ağıl içinde dolaşma, yemliğe gitme (navmesh), çit kırılırsa kaçma. *Bağımlı:* SOTF-S03 çit. *Kabul:* çit kırılan ağıldan hayvan çıkar ve dünyada dolaşır.
- [ ] **FS-A07 (P2)** — Gübre → tarla gübresi zinciri (FS-F05'e girdi).

### 4.6 Görevler — alet, makine, ormancılık

- [ ] **FS-E01 (P1)** — El aletleri: çapa, tırpan, orak, kova, tohum torbası; dayanıklılık ve çalışma genişliği (bir vuruşta işlenen hücre sayısı). *Kabul:* tırpan orağa göre 3 kat hücre hasat eder.
- [ ] **FS-E02 (P2)** — Hayvan veya insan çekişli alet (sapan/pulluk): çalışma genişliği büyük, stamina/yakıt maliyeti. İlk sürüm kapsam dışı olabilir.
- [ ] **FS-E03 (P1)** — Ormancılık bağlantısı: ağaç kesiminde oyuncu "hedef uzunluk" seçer (600/300/150) ve gövde buna göre bölünür; FS'deki kesim uzunluğu seçimi ile SOTF vuruş noktası kesimi aynı API. *Bağımlı:* SOTF-L04.

### 4.7 Alınmayacaklar

400+ lisanslı makine, gerçek marka ekonomisi, çok oyunculu kooperatif çiftlik, GPS otomatik dümen.

---

## 5. Subnautica — keşif ile açılan teknoloji, ortam limitleri, üs bütünlüğü

### 5.1 Mekanik ayrıştırması

1. **Ortam kaynağı:** Oksijen (dalış süresi; tüp ile artar), yiyecek, su, sağlık; ısı (termal bölgeler), radyasyon (giysi).
2. **Tarama ile açılma:** Parça (fragment) taranır; N parça taranınca tarif açılır. Veri tabanı girişleri bilgi verir. İlerleme oyuncunun nereye gittiğine bağlıdır.
3. **Biyom ve derinlik:** Her biyomun kendine özgü kaynakları, tehlikeleri ve derinlik aralığı; daha derin = daha iyi kaynak + ekipman zorunluluğu.
4. **Kaynak toplama:** Kaya yumruları (outcrop) kırılınca biyoma özgü olasılıkla cevher; büyük yataklar matkap gerektirir.
5. **Üretim istasyonları:** Fabrikatör, modifikasyon istasyonu, araç yükseltme konsolu; tarifler istasyon türüne bağlı.
6. **Üs inşası:** Modüller (koridor, oda, ay havuzu, tarama odası); gövde bütünlüğü derinliğe göre düşer, takviye ile artar, pencere/kapak düşürür; bütünlük negatifse su basar. Güç: güneş (derinlik/gün ışığı), termal (sıcak kaynak yakınında), biyoreaktör, nükleer.
7. **Navigasyon:** İşaret fenerleri (beacon), sinyaller, PDA haritası yoktur — oyuncu yön bulmak için işaret koyar.
8. **Hikâye kapıları:** Belirli bilgi/anahtar olmadan girilemeyen bölgeler.

### 5.2 Görevler — keşif ve açılma

- [ ] **SN-E01 (P0)** — Keşif noktası aktörü: kalıcı ID, etkileşim türü (inceleme, tarama, toplama), içerik anahtarı (bilgi metni), açılan şey (tarif, bölge, item). *Mevcut:* `RecordDiscovery(FName, FVector, double)` — etkileşim/açılma yok. *Kabul:* ikinci etkileşim ödül vermez; save/load sonrası açılan tarif açık kalır.
- [ ] **SN-E02 (P0)** — Parça tabanlı tarif açma: tarif `RequiredFragments=N`; her taranan/incelenen parça sayaç artırır; aynı parça iki kez saymaz. *Kabul:* 3 parçalı tarif 3. farklı parçada açılır; aynı parça tekrar taranınca sayaç değişmez.
- [ ] **SN-E03 (P1)** — Bilgi kaydı (günlük/defter): keşfedilen notlar oyuncu defterinde; bazı notlar yeni hedef/konum işaretler. *Kabul:* not okununca haritaya işaret eklenir.
- [ ] **SN-E04 (P1)** — Yer işareti (beacon/kazık+bez): oyuncu dünyaya işaret koyar; işaret mesafe ve isimle HUD'da görünür; inşa sistemi ile çubuk+bez'den yapılır. *Kabul:* işaret save/load'da korunur; kaldırılınca HUD'dan silinir.

### 5.3 Görevler — ortam limitleri ve ekipman kapısı

- [ ] **SN-L01 (P1)** — Bölge tehlike tablosu: bölge ID → tehlike türü (soğuk, karanlık, zehirli gaz, derin su, yükseklik) → gerekli ekipman → yoksa etki. *Kabul:* gerekli ekipman yoksa bölgede sağlık/ısı kaybı tabloya göre; ekipmanla etkisiz.
- [ ] **SN-L02 (P1)** — Nefes/oksijen analoğu (projeye uygun seçilmeli): su altı dalışı (göl/nehir) veya zehirli mağara gazı; kaynak süre ve ekipmanla uzar. Seçilmezse görev kapatılır. *Kabul:* süre bitince hasar; yüzeye çıkınca dolum.
- [ ] **SN-L03 (P1)** — Biyoma özgü kaynak: kaya yumrusu/maden noktası bölgeye göre olasılık tablosu ile item düşürür (deterministik seed). *Bağımlı:* 7D-T02, HYD-D görevleri. *Kabul:* aynı seed ile aynı yumru aynı item'ı verir.

### 5.4 Görevler — üs bütünlüğü ve güç

- [ ] **SN-B01 (P1)** — Barınak bütünlüğü (projeye uyarlanmış): oda modülünün su geçirmezlik/rüzgâr geçirmezlik puanı; açıklık (pencere/kapı) puanı düşürür, sızdırmazlık (kil sıva, yosun, bez) artırır; puan iç sıcaklığı belirler. *Bağımlı:* SOTF-B05, SURV ısı. *Kabul:* sıvalı kulübe sıvasız kulübeden daha sıcak; pencere açılınca iç sıcaklık düşer.
- [ ] **SN-B02 (P2)** — Güç kaynağı türleri ve konum bağımlılığı: su çarkı yalnız akarsuda, rüzgâr yalnız açık tepede (Hydroneer/Satisfactory güç ortak). *Kabul:* durgun suda su çarkı 0 güç.
- [ ] **SN-B03 (P2)** — İç mekân bitki yetiştirme (saksı/tarla kutusu): FS-F hücre modelinin küçük örneği. *Kabul:* kış aylarında iç mekân saksısında büyüme devam eder (ısıtma şartıyla).

### 5.5 Alınmayacaklar

Su altı açık dünya, araçlar (Seamoth/Cyclops), dev yaratık AI'ı, öncü teknolojisi hikâyesi.

---

## 6. Frostpunk — ısı, yakıt, kıtlık ve topluluk kararları

### 6.1 Mekanik ayrıştırması

1. **Isı alanı:** Merkezi jeneratör yarıçap içinde ısı verir; ısı kademeleri (dondurucu, çok soğuk, soğuk, serin, rahat); ısıtıcılar ve buhar merkezleri ısıyı uzaklara taşır; yalıtım araştırmaları binaların iç ısısını artırır.
2. **Yakıt ekonomisi:** Kömür tüketimi ısı kademesiyle artar; aşırı yükleme (overdrive) daha fazla ısı + jeneratör stresi; yakıt bitince şehir donar.
3. **Kaynak zincirleri:** Kömür (yığınlar, delici, maden), odun (sandıklar, bıçkıhane), çelik, ham yiyecek → pişmiş tayın.
4. **Nüfus ve iş gücü:** İşçi/mühendis/çocuk; iş yerlerinde vardiya; uzatılmış ve acil vardiya; barınma eksikliği hastalık getirir.
5. **Hastalık:** Soğuk ve iş yükü → hastalık; tedavi kapasitesi; ağır hastalık ve kalıcı sonuç.
6. **Umut ve hoşnutsuzluk:** Kararlar ve kanunlar iki metriği kaydırır; metrik eşikleri oyun sonu.
7. **Keşif seferleri:** Haritada gruplar gönderilir; kaynak, hayatta kalan, olay bulunur.
8. **Fırtına:** Öngörülen sıcaklık düşüşü takvimi ve oyuncunun hazırlanması.

### 6.2 Görevler — ısı modeli (tüm survival sistemlerinin ortak çekirdeği)

- [ ] **FP-H01 (P0)** — Isı kaynağı bileşeni: `HeatOutput`, `Radius`, `FuelConsumptionPerHour`, `FuelItemTags`, `IsLit`. Kamp ateşi, ocak, şömine, fırın aynı bileşen. *Bağımlı:* SURV-01 tek otorite. *Kabul:* aynı bileşen hem pişirme istasyonu hem barınak ısıtıcısı olarak çalışır; yakıt bitince söner.
- [ ] **FP-H02 (P0)** — Hissedilen sıcaklık = hava sıcaklığı (saat+mevsim+hava) + rüzgâr soğutması − barınak koruması + ısı kaynakları + giysi yalıtımı. Tek fonksiyon, tüm sistemler okur. *Mevcut:* `UpdateProtection(ClothingInsulation, ShelterProtection)` iki serbest sayı. *Kabul:* giysi ve barınak değerleri item/aktörden türetilir; elle set edilemez.
- [ ] **FP-H03 (P0)** — Isı kademeleri: sürekli sıcaklık değerini 5 kademeye bölen tablo; HUD'da kademe gösterimi. *Kabul:* kademe sınırları veri tablosunda.
- [ ] **FP-H04 (P1)** — Isı yayılımı oda bazlı: kapalı odada (SN-B01 bütünlüğü) ısı birikir, açık alanda yarıçapla düşer. *Kabul:* aynı ateş kapalı odada açık havadan daha yüksek iç sıcaklık üretir.
- [ ] **FP-H05 (P1)** — Aşırı yakma: ateşe fazla yakıt eklemek ısıyı artırır ama yakıtı hızlı tüketir ve ocak/şömine aşınır (yangın riski P2).

### 6.3 Görevler — yakıt ve kıtlık

- [ ] **FP-F01 (P0)** — Yakıt tanımları: takoz, çubuk, kütük, kömür (P2), saman; her birinin enerji değeri, yanma süresi ve nem cezası. *Bağımlı:* SOTF-L07, SOTF-L12. *Kabul:* 1 kuru takoz = tablo saat ısı; yaş takoz %X.
- [ ] **FP-F02 (P1)** — Günlük yakıt bütçesi görünümü: mevcut stok ÷ tüketim = kalan saat; HUD veya defterde. *Kabul:* sayı authoritative stoktan hesaplanır.
- [ ] **FP-F03 (P1)** — Soğuk dalgası olayı: takvimde önceden duyurulan (keşif/hava belirtisi ile) sıcaklık düşüşü; hazırlık süresi. *Kabul:* olay 2 gün önceden belirti verir; olay sırasında sıcaklık tabloya göre düşer.

### 6.4 Görevler — topluluk (MyProject3 ölçeğine indirgenmiş)

- [ ] **FP-P01 (P2)** — Küçük yerleşimci grubu (2–5 NPC): her birinin SURV profili; barınak ve yemek ihtiyacı; iş ataması (kesim, taşıma, tarla). *Bağımlı:* SOTF-A01 yardımcı AI. *Kabul:* barınaksız NPC hastalanır.
- [ ] **FP-P02 (P2)** — Moral metriği: yemek çeşidi, barınak sıcaklığı, iş yükü moral değerini kaydırır; düşük moral iş hızını düşürür.
- [ ] **FP-P03 (P2)** — Karar olayları: kıtlıkta tayın kesme / ağır iş vardiyası gibi oyuncu seçimleri; her seçim kalıcı kayıt ve ölçülebilir etki.
- [ ] **FP-X01 (P2)** — Keşif seferi: NPC veya oyuncu haritada uzak noktaya gönderilir, süre sonunda kaynak/olay döner (DS kargo + SN keşif ortak).

### 6.5 Alınmayacaklar

Şehir ölçeği, kanun ağaçları, otomaton, büyük nüfus simülasyonu.

---

## 7. The Long Dark — bireysel hayatta kalmanın tüm ayrıntısı

### 7.1 Mekanik ayrıştırması

1. **Beş durum çubuğu:** Sıcaklık (warmth), yorgunluk, susuzluk, açlık (kalori) ve bunların hepsinin bağlı olduğu genel sağlık (condition). Biri sıfıra inince sağlık düşer.
2. **Kalori ekonomisi:** Her faaliyet kalori yakar (yürüme, koşma, taşıma, kesme); yemekler kalori + su değeri verir.
3. **Hissedilen sıcaklık:** Hava sıcaklığı + rüzgâr soğutması (yönlü) + giysi + ateş + iç mekân bonusu.
4. **Giysi katmanları:** Baş, gövde iç/dış, eller, bacak iç/dış, ayak iç/dış, aksesuar. Her parçanın sıcaklık, rüzgâr geçirmezlik, ıslak/kuru durumu, donma durumu, hareket cezası, ağırlık ve dayanıklılığı vardır. Islak giysi daha az ısı verir ve donar; ateş başında kurur. Kumaş/deri ile onarılır.
5. **Ateş yakma:** Çıra + yakıt + (hızlandırıcı) + tutuşturucu (kibrit, çakmak taşı, büyüteç). Başarı olasılığı beceriye ve rüzgâra bağlıdır; ateşin süresi eklenen yakıtla uzar; rüzgâr ateşi söndürebilir; ateş yırtıcıları uzak tutar.
6. **Yiyecek ve su:** Çiğ et → pişmiş et (hastalık riski), bozulma yüzdesi, kar eritme → içilemez su → kaynatma → içilebilir su.
7. **Uyku:** Yatak/uyku tulumu; soğukta uyumak tehlikeli; uyku saati geçirir ve yorgunluğu giderir.
8. **Rahatsızlıklar (afflictions):** Hipotermi riski (birikimli), donma, gıda zehirlenmesi, dizanteri, burkulma, yanık, enfeksiyon riski, kan kaybı, kırık kaburga, parazit. Her biri özel tedavi ve süreye sahiptir.
9. **Av ve karkas:** Hayvan öldürülür, karkastan et/post/bağırsak çıkarılır (süre, alet); donmuş karkas yavaş işlenir; post ve bağırsak içeride kurutulur.
10. **Ekipman bozulması:** Alet ve giysi kullanımla ve zamanla bozulur.
11. **Yük sınırı:** Belirli kütlenin üstünde aşırı yük, hız ve kalori cezası.

### 7.2 Görevler — durumlar ve kalori

- [ ] **TLD-S01 (P0)** — Kalori modeli: açlık bir yüzde değil kalori deposu; bazal tüketim + faaliyet tüketimi (aktivite maliyet tablosu: yürüme, koşma, taşıma kütlesi, balta vuruşu, kazma, sürme). *Mevcut:* `FWorldSurvivalProfile` hunger alanı. *Bağımlı:* SURV-08. *Kabul:* 2 kütükle 1 km yürüme boş yürümeden tablo oranında fazla kalori yakar.
- [ ] **TLD-S02 (P0)** — Su: susuzluk deposu; su kaynakları (göl, yağmur toplayıcı, kar); içilemez su riski. *Kabul:* göl suyunu kaynatmadan içmek hastalık riski taşır (tablo).
- [ ] **TLD-S03 (P0)** — Genel sağlık bağlantısı: herhangi bir depo sıfırsa sağlık dakika başına düşer; hepsi doluyken yavaş iyileşir. *Kabul:* aç ve susuz karakter iki katı hızda sağlık kaybeder.
- [ ] **TLD-S04 (P1)** — Yorgunluk ve uyku: yorgunluk faaliyetle artar; uyku eylemi oyun saatini ilerletir (shared clock tek kez); soğukta uyurken sıcaklık düşmeye devam eder ve uyandırır. *Bağımlı:* SURV-07. *Kabul:* 8 saat uyku saati 8 saat ilerletir, tarım/üretim de 8 saat ilerler ve çift sayılmaz.

### 7.3 Görevler — giysi

- [ ] **TLD-C01 (P0)** — Giysi yuvaları ve katmanlar: 10 yuva (baş, gövde iç/dış, el, bacak iç/dış, ayak iç/dış, 2 aksesuar). *Kabul:* aynı yuvaya iki parça giyilemez.
- [ ] **TLD-C02 (P0)** — Giysi özellikleri: sıcaklık, rüzgâr geçirmezlik, ıslanma oranı, donma, hareket cezası, kütle, dayanıklılık. Toplam yalıtım bunların toplamı. *Bağımlı:* FP-H02. *Kabul:* rüzgârlı havada rüzgâr geçirmez mont, aynı sıcaklıktaki kazağa göre daha yüksek hissedilen sıcaklık verir.
- [ ] **TLD-C03 (P1)** — Islanma ve kuruma: yağmur, kar ve suya girme giysiyi ıslatır; ıslak giysi ısı kaybı; ateş başında kuruma süresi; ıslak giysi donar. *Kabul:* ıslak çorap donunca ayak donma riski başlar.
- [ ] **TLD-C04 (P1)** — Giysi onarımı ve üretimi: post/bağırsak/kumaş + iğne ile; hayvan postundan giysi tarifleri. *Bağımlı:* TLD-H01 karkas.

### 7.4 Görevler — ateş

- [ ] **TLD-F01 (P0)** — Ateş yakma eylemi: gerekli bileşenler (çıra + yakıt + tutuşturucu), başarı olasılığı (beceri, rüzgâr, ıslak malzeme), süre; başarısızlıkta tüketilen bileşenler kuralı. *Bağımlı:* FP-H01. *Kabul:* rüzgârlı açık alanda başarı oranı, rüzgârsız barınaktakinden düşük (tablo); başarısız denemede kibrit tükenir, yakıt kalır.
- [ ] **TLD-F02 (P0)** — Ateşin yaşamı: kalan süre, eklenen yakıtla uzar (maksimum sınır); söner; kül/köz kalır ve kısa süre yeniden yakılabilir. *Kabul:* yeniden yakma közden kibritsiz olur (süre sınırlı).
- [ ] **TLD-F03 (P1)** — Rüzgâr ve yağmur ateşi etkiler: korunaksız ateş rüzgârda sönme riski; çatı/taş ocak korur. *Kabul:* taş ocaklı ateş rüzgârda sönmez.
- [ ] **TLD-F04 (P1)** — Ateş = ışık + yırtıcı caydırıcı + pişirme + kurutma + su kaynatma (tek aktör, beş işlev).

### 7.5 Görevler — yemek, su ve bozulma

- [ ] **TLD-N01 (P0)** — Bozulma: her yiyecek item'ının kondisyon yüzdesi; mantıksal zamanla düşer; düşük kondisyon zehirlenme riski. Soğuk depo bozulmayı yavaşlatır. *Bağımlı:* SURV-09, CLOCK. *Kabul:* aynı et sıcak odada soğuk depoya göre tablo oranında hızlı bozulur; save/load zamanı çift saymaz.
- [ ] **TLD-N02 (P0)** — Pişirme: çiğ → pişmiş dönüşümü ateş/ocak üzerinde süre ile; pişmeden yemek hastalık riski; yanma (fazla bekleme). *Kabul:* çiğ et yemek X% gıda zehirlenmesi riski; pişmiş et 0.
- [ ] **TLD-N03 (P1)** — Su işleme: kar/göl → kap içinde ısıtma → içilebilir su. Kap türü (kap, tencere) gerekir. *Kabul:* kapsız su kaynatılamaz.
- [ ] **TLD-N04 (P1)** — Kurutma rafı: et/ot/post kurutma; kurutulmuş et uzun ömürlü. *Bağımlı:* SOTF-B09.

### 7.6 Görevler — rahatsızlıklar

- [ ] **TLD-A01 (P0)** — Rahatsızlık sistemi: data-driven tanım (tetikleyici, belirti etkisi, süre, tedavi item'ı, tedavi süresi, iyileşmeme sonucu). Tek bileşen, tüm oyunlar kullanır (7D debuff'ları dahil). *Kabul:* yeni rahatsızlık yalnız veriyle eklenir.
- [ ] **TLD-A02 (P0)** — İlk set: hipotermi (birikimli risk → aktif hastalık), donma (vücut bölgesi), gıda zehirlenmesi, burkulma (ağır yükle eğimde düşme), kanama. *Kabul:* her biri için tetik → etki → tedavi zinciri bir test senaryosunda gösterilir.
- [ ] **TLD-A03 (P1)** — Enfeksiyon riski: yara + dezenfeksiyon yoksa risk yüzdesi zamanla artar; antiseptik/bitki çayı ile sıfırlanır.
- [ ] **TLD-A04 (P1)** — Tedavi item'ları: bandaj (bez/yosun), atel (çubuk + bez — STICK zinciri), bitki çayları (toplanan bitki + su + ateş).

### 7.7 Görevler — av ve karkas

- [ ] **TLD-H01 (P1)** — Karkas aktörü: et kütlesi, post, bağırsak; işleme süresi alete ve karkasın donmuşluğuna bağlı; kısmi işleme (yarıda bırakılabilir). *Kabul:* donmuş karkas işleme süresi tablo katında uzun; yarım işlenmiş karkas save/load'da korunur.
- [ ] **TLD-H02 (P1)** — Post/bağırsak kurutma: içeride N gün; dışarıda bozulur. *Kabul:* kurumuş post giysi tarifine girer.
- [ ] **TLD-H03 (P2)** — Yırtıcı ve av hayvanı davranışı: tavşan/geyik kaçar, kurt kokuyu (taşınan et) takip eder, ateşten kaçar. *Bağımlı:* RAID AI altyapısı (aynı navigasyon/hedefleme).

### 7.8 Görevler — ekipman ve yük

- [ ] **TLD-E01 (P0)** — Alet dayanıklılığı: her kullanımda düşüş (ağaç kesme, kazma); %0'da kırılır; bileme taşı/onarım. *Kabul:* kırılan balta ile kesim reddedilir.
- [ ] **TLD-E02 (P1)** — Yük sınırı: toplam taşınan kütle (envanter + elde) eşiği; üstünde hız ve kalori cezası; ikinci eşikte koşamama. *Bağımlı:* DS-C02 ile aynı sistem. *Kabul:* tek kütle hesabı, iki sistemde iki sayı yok.

### 7.9 Alınmayacaklar

Hikâye modu bölümleri, buz kırma ve tırmanma halatı ayrıntıları (DS halatı ile birleştirilebilir), sahip olunan haritanın elle çizilmesi (P2'ye kadar).

---

## 8. Hydroneer — kazı, tabaka, ayıklama, modüler makine

### 8.1 Mekanik ayrıştırması

1. **Kazı:** Kürek ile arazi kazılır, kazılan toprak bir **fiziksel madde** olarak alınır (kova, kürek ucu); derinleştikçe farklı tabakalara inilir; en altta kazılamaz taban.
2. **Ayıklama (panning):** Toprak tavaya konur, suda çalkalanır; olasılıkla cevher ve değerli taş çıkar. Tabaka/bölge olasılığı belirler.
3. **Tipler:** Bölgelere göre farklı cevher türleri ve değerli taşlar; değer ve kütle farklı.
4. **Su:** Su borular, pompalar ve vanalarla taşınır; makineler (yıkayıcı, oluk) su gerektirir.
5. **Güç:** Jeneratörler, su çarkı; güç hatları; mantık kapıları.
6. **Makine modülerliği:** Makineler çerçeve/ızgaraya monte edilen modüllerden kurulur; bant, sıralayıcı, kırıcı, matkap.
7. **Eritme ve kalıp:** Cevher → fırın → erimiş metal → kalıp (külçe, levha, parça).
8. **Fizik nesneleri:** Her şey fiziksel taşınır, istiflenir, düşebilir; arazi satın alınır.

### 8.2 Görevler — kazı ve tabaka

- [ ] **HYD-D01 (P0)** — Kazı cebi tabaka tablosu: derinlik aralığı → malzeme (humus, toprak, kil, çakıl, taş, sert taş) → sertlik → gerekli alet → verim item'ı. *Mevcut:* `WorldTerrainStateSubsystem` `DepthLayer`, `MaxEditableDepthLayers=4`, `MaxDepthPerCell=8`. *Kabul:* katman 0–1 kürekle, 2–3 kazma ile; 4'ten derin red.
- [ ] **HYD-D02 (P0)** — Kazı = hacim çıkarma + item doğurma tek işlemde: hücre `Excavate` başarılı olursa toprak item'ı (veya kova doluluğu) doğar; başarısızsa ikisi de olmaz. *Bağımlı:* TERR-22, TXN. *Kabul:* aynı OperationId tekrarında ikinci toprak doğmaz.
- [ ] **HYD-D03 (P0)** — Doldurma: taşınan toprak item'ı hücreye geri konur (`Fill`), item tüketilir; malzeme kimliği korunur (kil ile doldurulan çukur kil olur). *Kabul:* doldurma toprak item'ı olmadan reddedilir.
- [ ] **HYD-D04 (P1)** — Kova/el arabası: dökme malzemeyi miktar olarak taşıyan kap; dolu kap kütlesi içerikten. *Kabul:* dolu kova kütlesi item tanımı × miktar.
- [ ] **HYD-D05 (P1)** — Kazı görsel ve collision güncellemesi (sınırlı cep içinde). *Bağımlı:* TERR-17/19 (render/collision adaptörü). *Kabul:* kazılan hücreye karakter iner; doldurulan hücrede yürür.

### 8.3 Görevler — ayıklama ve cevher

- [ ] **HYD-P01 (P1)** — Ayıklama eylemi: toprak + tava + su kaynağı (göl/dere/yalak) → olasılık tablosu (bölge × tabaka) → cevher/taş item veya boş. Deterministik seed + OperationId. *Kabul:* aynı seed ve işlem kimliği aynı sonucu verir; su yoksa red.
- [ ] **HYD-P02 (P1)** — Cevher tipleri (dar set): bakır, demir, (P2: altın); her birinin kütle ve değer verisi. *Kabul:* demir cevheri eritme tarifine girer.
- [ ] **HYD-P03 (P2)** — Oluk/yıkama düzeneği: kütük oluk (`GutterLog`) + su akışı ile toplu ayıklama — kütük sistemiyle doğrudan bağlantı. *Kabul:* oluğa su verilince içine dökülen toprak otomatik ayıklanır.

### 8.4 Görevler — su, güç, makine

- [ ] **HYD-W01 (P2)** — Su ağı (en küçük biçim): kaynak (dere/kuyu) → kanal/oluk zinciri → tüketici (oluk, değirmen, tarla sulama). Akış miktarı ve yön; boru basıncı yok (sadeleştirme). *Bağımlı:* SOTF-L (gutter/trough). *Kabul:* kanal kesilirse tüketici su almaz.
- [ ] **HYD-W02 (P2)** — Su çarkı: akan suyun yanında üretilen mekanik güç; değirmen/bıçkı tüketir. *Kabul:* su akışı yoksa güç 0.
- [ ] **HYD-M01 (P2)** — Modüler makine çerçevesi: kütük çerçeveye modül (taş değirmen, kırıcı, bıçkı) takılır; giriş/çıkış yuvaları. *Bağımlı:* PROD port sistemi.
- [ ] **HYD-S01 (P1)** — Eritme ve kalıp: cevher + yakıt → fırın (taş/kil yapı) → kalıp → külçe/basit parça (çivi, balta başı). *Bağımlı:* FP-F01 yakıt. *Kabul:* demir balta başı + çubuk sapı → demir balta (taş baltadan yüksek kalite, 7D-W01).

### 8.5 Alınmayacaklar

Arazi satın alma ekonomisi, mantık kapıları, sınırsız makine kombinasyonu.

---

## 9. Satisfactory — ölçülebilir üretim, taşıma kapasitesi, güç

### 9.1 Mekanik ayrıştırması

1. **Kaynak düğümleri ve saflık:** Saf olmayan/normal/saf düğüm; madenci kademesi çıkarım hızını belirler.
2. **Dakika başına tarif:** Her tarif giriş ve çıkışı dakika başına oranla tanımlar; makine ya tam hızda, ya girdi eksikliğinde ya da çıktı tıkalı olduğunda durur.
3. **Bant kapasitesi:** Konveyör kademeleri sabit hızlardadır (Mk1–Mk6: 60, 120, 270, 480, 780, 1200 öğe/dk); ayırıcı, birleştirici, akıllı ayırıcı (filtre), asansör.
4. **Sıvılar:** Boru akışı, pompa ve yükseklik.
5. **Güç ağı:** Üretim/tüketim dengesi; aşırı tüketimde sigorta atar ve tüm ağ durur; akü depolama; direk bağlantı limitleri.
6. **Aşırı hızlandırma:** Makine hızı artırılabilir, güç tüketimi doğrusal olmayan biçimde artar.
7. **İlerleme:** Kilometre taşları, araştırma, alternatif tarifler.
8. **İnşa ızgarası:** Temel karolar (ör. 8 m), duvar/rampa/kiriş snap; yapısal bütünlük yok, söküm tam iade.

### 9.2 Görevler — tarif ve makine

- [ ] **SF-R01 (P0)** — Tarif data asset: girdiler (item, miktar), çıktılar (ana + yan ürün), süre, istasyon türü, gereken yakıt/güç, gerekli alet/kalite. Tüm üretim (crafting, istasyon, ocak, değirmen, fırın) aynı tarif şeması. *Mevcut:* `FWorldProductionRecipe`, `InventoryCraftingComponent`. *Kabul:* yeni tarif kod değiştirmeden eklenir; crafting ve istasyon aynı şemayı okur.
- [ ] **SF-R02 (P0)** — Makine durum makinesi: `Idle`, `Working`, `StarvedInput`, `BlockedOutput`, `NoPower/NoFuel`, `Faulted`. Her durum HUD/istasyon panelinde sebep ile. *Kabul:* çıktı yuvası doluyken makine `BlockedOutput` olur ve girdi tüketmez.
- [ ] **SF-R03 (P1)** — Oran görüntüleme: tarif ekranında dakika (veya oyun saati) başına giriş/çıkış; istasyonun gerçek ortalama verimi (son N dakika). *Kabul:* girdi yarı hızda geliyorsa verim %50 gösterilir.
- [ ] **SF-R04 (P1)** — Kaynak noktası saflığı: maden/taş ocağı/kil yatağı düğümleri saflık kademesi; çıkarma hızı ve kalite. *Bağımlı:* 7D-T02, HYD-D01. *Kabul:* saf kil yatağı normal yataktan 2 kat hızlı (tablo).
- [ ] **SF-R05 (P2)** — Alternatif tarif: keşif (SN-E02) ile açılan ikinci tarif yolu (ör. kil tuğla vs taş blok duvar). *Kabul:* iki tarif aynı çıktıyı farklı girdilerle üretir.

### 9.3 Görevler — taşıma kapasitesi ve tampon

- [ ] **SF-L01 (P1)** — Port sistemi: istasyonların giriş/çıkış yuvası; yuva türü (katı, dökme, sıvı) ve kapasitesi. *Kabul:* sıvı yuvaya kütük bağlanamaz.
- [ ] **SF-L02 (P1)** — Fiziksel taşıma bağlantısı (projeye uygun ilk tür): el arabası/kızak hattı veya kütük oluk (su ile yüzdürme) veya zipline. Bağlantının dakika başına kapasitesi tanımlı. *Bağımlı:* DS-I04, SOTF-C03. *Kabul:* kapasiteden fazla girdi bekleyen tampona yığılır, kaybolmaz.
- [ ] **SF-L03 (P1)** — Tampon ve tıkanma: istasyon girdi/çıktı tamponu; dolunca üst akış durur (geri basınç). *Kabul:* depo dolunca zincirdeki tüm makineler sırayla `BlockedOutput` olur; depo boşalınca sırayla devam eder.
- [ ] **SF-L04 (P2)** — Ayırıcı/filtre analoğu: bir hat iki depoya kurala göre (item türü) dağıtır.

### 9.4 Görevler — güç ve yakıt ağı

- [ ] **SF-P01 (P2)** — Güç ağı (mekanik güç; su çarkı, rüzgâr, hayvan gücü): kaynaklar + tüketiciler + bağlantı (mil/kayış); üretim < talep ise tüm ağ yavaşlar veya durur (kural seçilir). *Bağımlı:* HYD-W02. *Kabul:* değirmen + bıçkı aynı su çarkına bağlanınca toplam talep tabloya göre karşılanır veya ikisi de yavaşlar.
- [ ] **SF-P02 (P1)** — Yakıtlı istasyonlar (fırın, ocak) ortak yakıt sistemini kullanır: yakıt yuvası, enerji değeri, kalan süre. *Bağımlı:* FP-H01, FP-F01. *Kabul:* ocak ile ısı kaynağı aynı yakıt API'sini çağırır.

### 9.5 Görevler — inşa ergonomisi ve ilerleme

- [ ] **SF-B01 (P2)** — Söküm iadesi kuralı: yerleştirilmiş üretim yapısı sökülünce malzemenin tamamı/oranı geri gelir (durumuna göre). *Bağımlı:* SOTF-D03.
- [ ] **SF-B02 (P2)** — Kilometre taşı/araştırma: belirli üretim veya keşif hedefleri yeni tarif kademesini açar (taş alet → demir alet). *Kabul:* demir eritme ilk demir cevheri ayıklanmadan açılmaz.

### 9.6 Alınmayacaklar

Uzay asansörü, trenler, kamyon rotası kaydı, nükleer, ölçeksiz fabrika.

---

## 10. Ortak sistem eşleştirme — bir çekirdek, birçok oyun

| Ortak sistem | Hangi oyunlardan beslenir | Tek otorite (önerilen sahip) | İlk görevler |
|---|---|---|---|
| Item tanımı (kütle, hacim, yuva, yakıt, bozulma, kalite) | Hepsi | `ItemDefinition` data asset + `WorldItemOwnershipSubsystem` | ITEM-01, DS-C01 |
| Taşıma ve yük | SOTF, DS, TLD, Hydroneer | `ULogCarryComponent` genelleştirilmiş taşıma bileşeni | SOTF-C01, DS-C02, TLD-E02 |
| Kesim ve işleme eylem grafiği | SOTF, FS ormancılık, 7D | Kütük eylem tablosu (SOTF-L01) | SOTF-L03/L04, FS-E03 |
| Yapısal destek ve çökme | SOTF, 7D, DS köprü, SN üs | `WorldStructuralGraphSubsystem` (çoklu destek) | SOTF-D01, 7D-S02 |
| Malzeme kademesi ve onarım | 7D, SOTF, DS bakım | Graph düğüm verisi | 7D-M01, 7D-M02 |
| Arazi kazı ve malzeme | 7D, Hydroneer, FS sürme, SN kaynak | `WorldTerrainStateSubsystem` + render adaptörü | HYD-D01/D02, FS-F03 |
| Isı ve hissedilen sıcaklık | Frostpunk, TLD, SOTF, SN | Tek sıcaklık fonksiyonu | FP-H01, FP-H02 |
| Yakıt | Frostpunk, TLD, Satisfactory, 7D, Hydroneer | Tek yakıt bileşeni | FP-F01, SF-P02 |
| Rahatsızlık ve tedavi | TLD, 7D, Frostpunk | Data-driven rahatsızlık bileşeni | TLD-A01 |
| Bozulma ve zaman | TLD, FS, SOTF | Shared clock + item kondisyonu | TLD-N01, CLOCK |
| Rota maliyeti | DS, 7D AI, SOTF kızak, Satisfactory taşıma | `RouteCostProvider` | DS-T01, 7D-R02 |
| Tarif ve istasyon | Satisfactory, FS, Hydroneer, 7D, SOTF crafting | Tek tarif şeması | SF-R01, SF-R02 |
| Keşif ve açılma | Subnautica, Frostpunk, 7D | Keşif aktörü + tarif kilidi | SN-E01, SN-E02 |
| AI navigasyon ve hedefleme | 7D baskın, SOTF yardımcı, TLD yırtıcı, DS hırsız, FS hayvan | Ortak AI hedef/yol katmanı | 7D-R02, SOTF-A01 |
| Barınak ve oda bütünlüğü | SOTF çatı, SN gövde, Frostpunk yalıtım, TLD iç mekân | Oda algılama + bütünlük puanı | SOTF-B05, SN-B01 |
| Su | FS sulama, Hydroneer ayıklama, TLD içme, DS geçiş | Su kaynağı + kap item'ları + kanal | TLD-N03, HYD-W01 |

Kural: Bir oyuna özel görev, tablodaki ortak sistem görevi bitmeden başlatılmaz (ör. FS-A01 yemlik, ITEM-01 ve SF-R01 olmadan başlamaz).

---

## 11. Önerilen sıra — "bir gün, bir döngü" dikey dilimleri

Her dilim bir öncekinin üstüne kurulur ve kendi kabul senaryosu ile kapanır.

**Dilim A — Orman ve kulübe (SOTF + TLD çekirdeği)**
SOTF-T01..T03, SOTF-L01..L06, SOTF-L14, SOTF-C01..C02, SOTF-B01..B03, SOTF-D01..D02, TLD-S01..S03, FP-H01..H03, TLD-F01..F02, TLD-E01.
*Kabul:* ağaç kes → kütükleri 2'şer taşı → yarım uzunluğa kes → 3 sıra duvar + kapı açıklığı → taş ocak → ateş yak → hissedilen sıcaklık yükselir → kalori ve dayanıklılık düşüşü ölçülür → save/load sonrası her şey aynı.

**Dilim B — Barınak ve kış (TLD + Frostpunk + SN barınak)**
SOTF-B04..B05, SOTF-L07, SOTF-L12, FP-F01..F03, TLD-C01..C03, TLD-N01..N02, TLD-A01..A02, SN-B01, TLD-S04.
*Kabul:* soğuk dalgası duyurulur → takoz stoklanır → çatı ve sıva ile oda bütünlüğü yükselir → ıslak giysi ateşte kurur → uyku saati ilerletir → hipotermi riski barınakta sıfırlanır.

**Dilim C — Kazı ve ilk metal (Hydroneer + 7D madencilik)**
TERR-11..13 ve render/collision adaptörü, HYD-D01..D05, HYD-P01..P02, 7D-T01, HYD-S01, SF-R01..R02.
*Kabul:* kazı cebinde katmanlara inilir → toprak item olur → derede ayıklanır → demir cevheri → fırın → balta başı → demir balta; kazı ve item tek işlemde, tekrarda çoğalmaz.

**Dilim D — Tarla ve hayvan (FS)**
FS-F01..F04, FS-F08, FS-G01..G03, FS-G07, FS-A01..A03, FS-P01.
*Kabul:* parsel sür → tohum ek → mevsimle büyür → hasat item olarak ambara → tavuk yemlikten yer → yumurta item'ı doğar → toplanmazsa bozulur.

**Dilim E — Taşıma ve geçiş (DS + Satisfactory taşıma)**
DS-C01..C03, DS-T01, DS-I01, DS-I03, DS-I05, SOTF-C03, SF-L02..L03.
*Kabul:* dere üstüne kütük köprü → rota kısalır → kızakla 6 kütük taşınır → eğimde yavaşlar → köprü kazıyla çökertilince rota yeniden hesaplanır.

**Dilim F — Baskın gecesi (7D)**
7D-M01..M02, 7D-S01..S03, 7D-R01..R03, 7D-D01, SOTF-L06.
*Kabul:* baskın gecesi duyurulur → AI en zayıf duvarı seçer → sivri kazık hattı hasar verir → duvar kırılır, üst sıra çöker → onarım kütükle yapılır.

**Dilim G — Keşif ve açılma (Subnautica + Frostpunk keşif)**
SN-E01..E04, SN-L01, SN-L03, SF-R05, FP-X01.

---

## 12. Kapsam dışı (bilinçli olarak)

- Çok oyunculu ve asenkron paylaşım (DS sosyal ağ, SOTF co-op).
- Motorlu araçlar (FS traktörleri, DS kamyonları, 7D araçları) — ilk erken erişim sürümünde yok.
- Büyük nüfus/şehir simülasyonu (Frostpunk tam ölçek).
- Uzay/elektrik/nükleer seviyesinde endüstri (Satisfactory son kademeler).
- Su altı açık dünya ve dev yaratıklar (Subnautica).

Bu listeye bir şey eklemek veya çıkarmak bir tasarım kararıdır ve bu belgede tarih ile kaydedilmelidir.
