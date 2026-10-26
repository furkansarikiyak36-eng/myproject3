# MyProject3 — 9 Referans Oyunun Derin Mekanik Analizi ve Görev Listesi (v2)

Tarih: 26 Ekim 2026
Kaynak taban: `origin/demo/vertical-slice-phase-01` (d1ebd78). Kod referansları bu daldandır.
Durum: Tasarım ve görev belgesidir. Hiçbir görev "yapıldı" sayılmaz; bitiş yalnızca **Kabul** satırı UE 5.8 içinde gösterildiğinde olur.

v2 değişikliği: Proje sahibinin yönlendirmesiyle her oyundan alınacak **asıl çekirdek** yeniden belirlendi:

| Oyun | Alınacak çekirdek | Alınmayacak |
|---|---|---|
| Sons of the Forest | Kütük **ve aynı derinlikte çubuk ve taş**: fiziksel malzeme, işleme, serbest inşa | Hikâye, mutantlar |
| 7 Days to Die | **Smooth voxel kazılabilir arazi** ve **baskınlar** | Blok/küp inşa sistemi, şekil menüsü, blok yükseltme |
| Death Stranding | **Yaşayan dünya**: yağmur, sel, nehir engeli; oyuncunun altyapıyla dünyayı **kalıcı** değiştirmesi | Asenkron çok oyunculu, BT/hikâye |
| Farming Simulator | **Tarım ve hayvancılık derinliği** | Lisanslı makine filosu |
| Subnautica | **Hikâyeyi birkaç notla anlatan, serbest bırakan ama isteyene araştırma imkânı veren** keşif | Su altı dünya, araçlar |
| Frostpunk + The Long Dark | **Hayatta kalmak için risk almak zorunda olma hissi** — her ölçekte ve atmosferde | Şehir yönetimi, kanun ağacı |
| Hydroneer + Satisfactory | **Üretim ağları: bir ham maddeden onlarca ürün** | Uzay/nükleer ölçek, trenler |

## 0. Okuma kuralları

- **Görev biçimi:** `ID (öncelik) — Ne yapılacak.` *Kabul:* gözlemlenebilir sonuç + en az bir negatif durum. *Mevcut:* projede bugün karşılığı. *Bağımlı:* önce bitmesi gereken görev.
- **Öncelik:** P0 = dikey dilim için zorunlu çekirdek, P1 = oynanış derinliği, P2 = genişleme.
- **Sayılar:** Oyunlardan yalnız kesin bilinen sayılar aktarılır; diğer her değer ayarlanabilir veri tablosu girdisidir.
- **Tek sahip kuralı:** Ağırlık, ısı, yakıt, dayanıklılık, kalite, destek, su seviyesi gibi kavramlar tek sistemde yaşar (Bölüm 9). Oyuna özel ikinci sayaç açılmaz.

## 0.1 Yedi tasarım sütunu

1. **Fiziksel malzeme:** Her ham madde (kütük, çubuk, taş, kil, lif, post, kemik) aynı sözleşmeye uyar: varyant eksenleri, işleme eylem grafiği, taşıma, yerleştirme, destek, depolama, bozulma.
2. **Yaşayan ve değiştirilebilir dünya:** Doğa kendi kendine değişir (yağmur, sel, kar, toprak kayması), oyuncu da dünyayı kalıcı değiştirir (köprü, yol, kazı, set). İki değişim aynı dünya durumuna yazılır ve birbirini etkiler.
3. **Risk zorunluluğu:** Güvenli kalmak sonsuza kadar mümkün değildir; yakın kaynaklar tükenir, hava pencereleri kapanır, oyuncu bilinçli risk alarak dışarı çıkar.
4. **Üretim ağları:** Her ham madde birden fazla ara ürüne, her ara ürün birden fazla son ürüne gider; yan ürünler başka zincirlerin girdisidir.
5. **Derin tarım ve hayvancılık:** Toprak, mevsim, tür ve bakım kararları ölçülebilir sonuç üretir.
6. **Notlarla anlatı:** Hikâye birkaç yerleştirilmiş nottan ve çevreden okunur; zorunlu görev yoktur.
7. **Baskın baskısı:** Dünya düzenli aralıklarla oyuncunun kurduğu düzeni sınar; savunma, arazi ve yapı bu sınavın parçasıdır.

---

## 1. Sons of the Forest — kütük, çubuk ve taş aynı derinlikte

### 1.1 Mekanik ayrıştırması

SOTF'de üç temel malzeme (kütük, çubuk, taş) aynı dünyada fiziksel olarak toplanır, taşınır, işlenir, serbestçe yerleştirilir ve depolanır. İnşa dört katmandan oluşur:

1. **Kaynak fiziği:** Ağaç baltayla kesilir ve devrilir; çubuklar yerden ve çalılardan toplanır; taşlar yerden toplanır. Her malzemenin taşıma biçimi farklıdır (omuzda kütük, demet hâlinde çubuk, elde taş); kızak toplu taşır; her malzemenin kendi tutucu aktörü vardır.
2. **İşleme:** Kütük vurulan noktadan kesilir ve uzunlamasına bölünür. Çubuk sivriltilir ve üretime girer. Taş fırlatılır, ocak/zemin/duvar olarak dizilir.
3. **Serbest inşa:** Kütükle duvar, direk, kiriş, zemin, çatı ve merdiven; çubukla çit, çerçeve ve küçük yapılar; taşla zemin, yol, duvar, basamak ve ocak. Hazır planlar (rehber kitap) da malzeme taşınarak tamamlanır.
4. **Yapı yaşamı:** Destek kaybında düşme, düşman saldırısı, yardımcı karakterin (Kelvin) toplama ve inşa komutları.

### 1.2 Ortak fiziksel malzeme sözleşmesi (kütük, çubuk, taş ve sonradan eklenecek her malzeme)

Her malzeme için aşağıdaki **sekiz alan** tanımlanmadan o malzeme "tamam" sayılmaz:

| # | Alan | İçerik |
|---|---|---|
| 1 | Varyant eksenleri | Uzunluk/boyut, kesit/şekil, kalınlık, nem, tür, durum |
| 2 | Kaynak | Dünyada nereden ve nasıl çıkar, bölgesel dağılım, yenilenme |
| 3 | Eylem grafiği | Hangi alet + hangi giriş durumu → hangi çıktı(lar); ara adım atlanamaz |
| 4 | Taşıma | Yuva tipi, adet sınırı, kütle, kızak/kap uyumu |
| 5 | Yerleştirme | Modlar, zemin/destek kuralı, snap/anchor tipleri, diğer malzemelerle birleşim |
| 6 | Destek | Kütle, taşıma kapasitesi, açıklık limiti, bağlantı gücü |
| 7 | Depolama | Tutucu aktörü, stok olarak saklanabilirlik, görsel doluluk |
| 8 | Yaşam | Bozulma, ıslanma, yanma, hasar, söküm verimi |

**Kod bulgusu (P0):** Bugün `AStickPrimitivePiece::TryConnectToAnchor` yalnız `AStickPrimitivePiece*`, `AStoneBuildingPiece::TryConnectToAnchor` yalnız `AStoneBuildingPiece*` kabul ediyor. Aktör düzeyinde kütük + çubuk + taş birleşimi mümkün değil. Çubuk ve taşta ölçü alanı ve mesh varyantı yok; repoda çubuk ve taş FBX'i yok.

### 1.3 Kütük varyant matrisi

| Varyant | Nasıl oluşur | Enum karşılığı | FBX | Açık |
|---|---|---|---|---|
| Tam kütük | Ağaçtan | `WholeLog` / `FullLength+FullRound` | `SM_RawLog_600`, `Long_01..08`, `Short_01..04` | `Long_*`/`Short_*` ailelerinin uzunluk/çapı tanımsız |
| 3/4 uzunluk | Enine kesim | `ThreeQuarterLog` | **Yok** (450 cm yok) | Enum var, mesh yok |
| 1/2 uzunluk | Enine kesim | `HalfLengthLog` | `SM_RawLog_300` | — |
| 1/4 uzunluk | Enine kesim | `QuarterLog` | `SM_RawLog_150` | — |
| 75 cm | Enine kesim | **Yok** | `SM_RawLog_75_*` (9 dosya) | Mesh var, enum yok |
| Yarım kesit | Boyuna yarma | `SplitHalfLog` / `HalfRound` | `*_Half` | — |
| Çeyrek kesit | Yarımı yarma | `QuarterSectionLog` | `*_Quarter_04` | `_04` anlamı belgesiz |
| Yontulmuş 1 yüz | Balta/keser | `HewnLog` / `HewnFlat` | `*_Hewn1` | — |
| Yontulmuş 2 yüz | İkinci yüz | **Yok** | `*_Hewn2` | Enum eksik |
| Kalas / kare kiriş | 4 yüz | `SquaredBeam` | `*_Kalas` | Hewn2 → Kalas geçişi yok |
| Plank | Kalastan dilimleme | `Plank` | `*_Plank` | Verim sayısı tanımsız |
| Oluk | Yarım kesite oluk | `GutterLog` | `*_Gutter` | Su taşıma işlevi yok |
| Yalak | Oyma | `TroughLog` | `*_Trough` | Su tutma işlevi yok |
| Çentikli | Uç çentik | `NotchedLog` | yalnız `75_FlatNotch_TopBottom` | Çentik tipleri yok |
| Birleşim parçası | Zıvana vb. | `JointPiece` | **Yok** | — |
| Sivri uç | Ucu yontma | **Yok** | **Yok** | Savunma duvarı ve tuzak için zorunlu |
| Açıklık kesikli duvar kütüğü | Duvara vurma | **Yok** | **Yok** | Pencere/kapı |
| Eğik uç | Açılı kesim | **Yok** | **Yok** | Çatı ve köşe |
| Yakacak takoz | Kısa parçayı yarma | **Yok** | **Yok** | Yakıt zincirine bağlantı |
| Kütük kalıntısı, dallar, kabuk, talaş | Kesim yan ürünü | **Yok** | **Yok** | Üretim ağının girdileri (Bölüm 7) |

Ortogonal eksenler: çap bandı (ince/orta/kalın), nem (yaş/kuru), durum (sağlam/hasarlı/yanık), tür (P2).

### 1.4 Çubuk varyant matrisi

Bugünkü kod: `EStickPrimitiveState {Raw, Broken, Sharpened, DoubleEndedStake, PrimitiveTool, Spear, Bow, Arrow, Torch, BarrierPart, StoragePart, TrapComponent, AssembledStructure, Salvaged}`. Bu liste **geometri durumu** (kırık, sivri) ile **ürünü** (yay, meşale, tuzak parçası) karıştırıyor. Çubuk bir malzeme; yay ve meşale ise o malzemeden üretilen ayrı item'lar olmalı.

| Eksen | Değerler | Etki |
|---|---|---|
| Kaynak | yerden ölü dal, çalı, devrilen ağacın dalları, kütükten yarılmış çıta, söğüt/fındık gibi esnek filiz (P1) | Kalite ve nem başlangıcı |
| Uzunluk | kısa (kırık, ~40 cm), standart (~100 cm), uzun sırık (~200+ cm) | Hangi tarife girer, taşıma adedi |
| Kalınlık | ince (dal), orta, kalın (sırık) | Dayanım, yakıt değeri, bükülebilirlik |
| Nem | yaş (esnek), kuru (sert, iyi yanar) | Yaş = örgü ve yay; kuru = yakıt, ok, kazık |
| Yüzey | kabuklu, soyulmuş | Soyulmuş = ok, alet sapı; kabuk = ip lifi (yan ürün) |
| Uç | düz, kırık, sivri, çift sivri, çatallı (Y), çentikli | Kazık, tuzak tetiği, raf, mızrak |
| Sertleştirme | yok, ateşte sertleştirilmiş | Kazık ve mızrak dayanımı |
| Durum | sağlam, çatlak, yanık, çürük | Söküm verimi, yakıt |

| Eylem | Giriş koşulu | Çıktı |
|---|---|---|
| Kır | standart veya uzun | 2 kısa (uzunluk toplamı korunur) |
| Sivrilt | bıçak/balta, kuru veya yaş | Sivri uç (tek veya çift) |
| Ateşte sertleştir | sivri uç + ateş, süre | Sertleştirilmiş sivri uç |
| Soy | bıçak | Soyulmuş çubuk + kabuk lifi |
| Çentik aç | bıçak | Çentikli uç (tuzak tetiği, raf) |
| Yar | balta, kalın | 2 çıta |
| Bük | yaş, ince/orta, esnek tür | Kavisli parça (çember, sepet, yay gövdesi) |
| Bağla | halat/lif + 2+ çubuk | Çerçeve, demet, üçayak |
| Ör | yaş ince çubuklar + kazıklar | Örgü (wattle) panel |

Çubuktan çıkan ürünler (ayrı item): kazık, çit kazığı, mızrak, ok şaftı, yay, meşale, alet sapı, kurutma rafı, üçayak, çerçeve, örgü panel, sepet, tuzak tetiği (4 figürü), atel, merdiven basamağı, çatı kirişi (ince), çıra, kızak.

### 1.5 Taş varyant matrisi

Bugünkü kod: `EStonePieceType {RawStone, Block, Slab, Column, Lintel, Corner, Other}`; ölçü, tür ve kütle alanı yok.

| Eksen | Değerler | Etki |
|---|---|---|
| Kaynak | yüzey taşı, dere çakılı, kaya kırma, ocak bloğu, kazıdan çıkan taş | Şekil ve tür dağılımı |
| Boyut bandı | çakıl (avuç), el taşı, iki elle taşınan, kaya (taşınamaz; kaldıraç/yuvarlama) | Taşıma yuvası, kızak kapasitesi, duvar dokusu |
| Tür | kireçtaşı, kumtaşı, granit, çakmaktaşı, dere taşı (yuvarlak) | Dayanım, işlenebilirlik, ürün ağı |
| Şekil | ham, kaba yontulmuş, blok, levha, köşe, sütun, lento, kama, basamak, kemer taşı, ocak taşı | Yerleşim uyumu |
| Durum | sağlam, çatlak, kırık, ısıl çatlamış | Söküm, dayanım |

| Eylem | Alet | Çıktı |
|---|---|---|
| Kır | taş/çekiç | Daha küçük boyut bandı parçalar + çakıl |
| Kaba yont | çekiç + keski | Kaba yontulmuş (düz bir yüz) |
| Şekillendir | keski, süre | Blok, levha, köşe, basamak, lento, kemer taşı |
| Yonga çıkar (knapping) | yalnız çakmaktaşı | Bıçak, ok ucu, kazıyıcı, kıvılcım taşı |
| Bile | kumtaşı + su | Alet keskinliği (dayanıklılık onarımı) |
| Yak | kireçtaşı + fırın + yakıt | Sönmemiş kireç → harç, sıva, badana |
| Öğüt | granit değirmen taşı | Un, toz (Bölüm 7) |

| Yerleşim tipi | Kural |
|---|---|
| Kuru duvar (harçsız) | Taşlar oturma yüzeyine göre yerleşir; alt sıradaki iki taşa basan taş daha güçlü (derz kaydırma) |
| Harçlı duvar | Harç item'ı tüketir, bağlantı gücü artar, kuruma süresi var |
| Zemin ve yol | Taş yol rota maliyetini düşürür, çamurlanmaz |
| Basamak | Eğimde yürünebilir yol |
| Ocak ve şömine | Isı kaynağının rüzgâr ve yağmur korumasını artırır |
| Temel | Üstüne kütük duvar oturur; arazi revizyonuna bağlı |
| İstinat duvarı | Kazı kenarında toprak kaymasını engeller (Bölüm 2) |
| Set ve baraj | Su seviyesini değiştirir (Bölüm 3) |
| Basamak taşları ve köprü ayağı | Nehir geçişi (Bölüm 3) |

### 1.6 Karma malzeme birleşimleri

| Birleşim | Malzemeler | İşlev |
|---|---|---|
| Taş temel + kütük duvar | Taş, kütük | Kütüğün yerden nem alıp çürümesini azaltır |
| Örgü + sıva (wattle and daub) | Yaş çubuk, kil, saman | Ucuz, yalıtımlı duvar; kil yoksa rüzgâr geçer |
| Çubuk çatı iskeleti + saman/kabuk kaplama | Çubuk, saman/kabuk | Hafif çatı; yağmuru keser |
| Kütük kiriş + plank zemin | Kütük, plank | Kat zemini |
| Taş ocak + kütük baca kirişi | Taş, kütük | Isı kaynağı + barınak |
| Kazık + halat + kütük | Çubuk, lif, kütük | Sal, köprü korkuluğu, merdiven |
| Taş ayak + kütük köprü | Taş, kütük | Nehir geçişi (Bölüm 3) |

### 1.7 Görevler — ortak malzeme altyapısı

- [ ] **SOTF-M01 (P0)** — Ortak `PhysicalMaterialPiece` arayüzü: kimlik, malzeme türü, varyant durumu, kütle, anchor listesi, sahiplik kaydı. Kütük, çubuk ve taş aktörleri bunu uygular. *Kabul:* tek bir yerleştirme fonksiyonu üç aktör türünü de kabul eder.
- [ ] **SOTF-M02 (P0)** — Malzemeler arası anchor uyumluluk tablosu: (malzeme A anchor tipi, malzeme B anchor tipi) → bağlantı izni + bağlantı gücü. `TryConnectToAnchor` türden bağımsız hâle gelir. *Kabul:* taş temele kütük duvar bağlanır; çubuk kazığa taş blok bağlanamaz (tabloda yok).
- [ ] **SOTF-M03 (P0)** — Malzeme başına varyant durum yapısı ve `ParseLegacy()` (kütük için `ELogPieceType`, çubuk için `EStickPrimitiveState`). *Kabul:* eski kayıt yeni yapıya dönüşür; bilinmeyen değer yüklemeyi durdurur.
- [ ] **SOTF-M04 (P0)** — Çubukta geometri ile ürünü ayır: `StickPiece` yalnız malzeme durumunu tutar; yay, meşale, mızrak, ok vb. ayrı item tanımı olur. *Kabul:* "yay" artık bir çubuk durumu değil, tarifle üretilen item.
- [ ] **SOTF-M05 (P0)** — FBX manifest scripti (kütük, çubuk, taş için): bounds, ana eksen, pivot, ölçülen uzunluk/boyut, bağlı varyant. *Kabul:* her varyant satırının mesh'i var, her mesh'in varyantı var; sapma varsa script hata verir.
- [ ] **SOTF-M06 (P1)** — Çubuk ve taş mesh seti üretimi (en az: çubuk 3 uzunluk × 2 kalınlık + sivri/çift sivri/çatallı; taş 4 boyut bandı × ham + blok/levha/köşe/basamak). *Bağımlı:* SOTF-M05.

### 1.8 Görevler — ağaç ve hasat

- [ ] **SOTF-T01 (P0)** — Ağaç verisi data asset'e taşınır: gövde uzunluğu, çap bandı, vuruş sayısı, devrilme kuralı, kütük dizisi, dal sayısı, kabuk/reçine verimi. *Mevcut:* `AHarvestableTreeActor::SpawnHarvestLogs`. *Kabul:* iki ağaç aynı kodla farklı çıktı verir; veri eksikse kesilir ama çıktı vermez ve hata loglar.
- [ ] **SOTF-T02 (P0)** — Vuruş yönüne göre devrilme yönü; devrilme yolundaki yapıya `ApplyDamage`. *Kabul:* eve doğru devrilen ağaç duvara hasar verir.
- [ ] **SOTF-T03 (P0)** — Kütük kalıntısı: kalıcı kimlik, inşayı engeller, sökülebilir. *Kabul:* save/load sonrası yerinde; üstüne temel konamaz.
- [ ] **SOTF-T04 (P0)** — Dallar → çubuk: devrilen ağaçtan veri kadar çubuk (uzunluk/kalınlık dağılımı ile). *Kabul:* aynı ağaçtan ikinci kez çubuk çıkmaz.
- [ ] **SOTF-T05 (P1)** — Yenilenme: kalıntıdan fidan, fidandan ağaç (shared clock); ölü dal ve yüzey taşı yenilenme hızı ayrı. *Kabul:* kamp çevresindeki ölü dal tükendikten sonra veri süresince yenilenmez (Bölüm 6 risk zorunluluğuyla bağlantılı).

### 1.9 Görevler — kütük işleme

- [ ] **SOTF-L01 (P0)** — #24/#25 kararından sonra kanonik durum: `{LengthCm, CrossSection, FaceCount, EndShape, Notches[], Openings[], Moisture, Condition}`. *Bağımlı:* REL-03.
- [ ] **SOTF-L02 (P0)** — 75 cm ve 3/4 tutarsızlığını kapat (enum ekle veya mesh üret veya işlemi kaldır).
- [ ] **SOTF-L03 (P0)** — Yontma zinciri: Round → Hewn1 → Hewn2 → Kalas → Plank×N; adım atlanırsa red. *Kabul:* tam yuvarlağa doğrudan Kalas reddedilir, hiçbir şey değişmez.
- [ ] **SOTF-L04 (P0)** — Vuruş noktasına göre enine kesim; uzunluk toplamı korunur. *Kabul:* 600'ü %50'den kesmek 300+300 verir.
- [ ] **SOTF-L05 (P0)** — Kesim ilerlemesi: vuruş başına artış, çentik decal'ı, eşik dolmadan geometri değişmez, kayıtta tutulur.
- [ ] **SOTF-L06 (P0)** — Sivri uç eylemi (savunma kazığı). *Kabul:* sivri uçlu dikili kütük temas hasarı verir.
- [ ] **SOTF-L07 (P1)** — Yakacak takoz (≤150 cm parçadan). *Bağımlı:* Bölüm 6 yakıt.
- [ ] **SOTF-L08 (P1)** — Çentik tipleri (düz üst/alt, eyer, bindirme); köşe kilidi. *Kabul:* eyer çentikli iki kütük 90° köşede kilitlenir; çentiksiz yalnız yaslanır.
- [ ] **SOTF-L09 (P1)** — Duvara açıklık kesimi; graph'ta segmentlere bölme.
- [ ] **SOTF-L10 (P1)** — Eğik uç kesimi (30/45/60°).
- [ ] **SOTF-L11 (P1)** — Plank verimi çap bandına göre tablo; hacim dengesi.
- [ ] **SOTF-L12 (P1)** — Nem: yaş kütük kurur, yağmur ıslatır; ağırlık ve yakıt değeri nemle değişir.
- [ ] **SOTF-L13 (P1)** — Kesim yan ürünleri: kabuk, talaş, reçine item'ları (Bölüm 7 girdisi). *Kabul:* yontma işlemi talaş üretir; kabuk soyma kabuk verir.

### 1.10 Görevler — çubuk

- [ ] **SOTF-K01 (P0)** — Çubuk toplama: yerden ölü dal (kuru), çalıdan (yaş), dal yığınından; her kaynak uzunluk/kalınlık/nem dağılımıyla. *Kabul:* çalıdan alınan çubuk yaş, yerdeki ölü dal kuru başlar.
- [ ] **SOTF-K02 (P0)** — Demet taşıma: çubuklar tek tek değil demet hâlinde taşınır; demet adedi kalınlığa göre (ör. ince 10, kalın 3). Demet düşerse çubuklar tek tek dünyaya saçılır. *Kabul:* demet düşünce her çubuk ayrı sahiplik kaydıyla dünyada.
- [ ] **SOTF-K03 (P0)** — Eylem grafiği (1.4 tablosu) kodda: kır, sivrilt, ateşte sertleştir, soy, çentik aç, yar, bük, bağla, ör. Her eylem giriş koşulunu kontrol eder. *Mevcut:* `TryProcessTo(EStickPrimitiveState)`. *Kabul:* kuru çubuk bükülmek istenince red; yaş çubuk bükülür.
- [ ] **SOTF-K04 (P0)** — Çubuk yerleştirme: dikey kazık (zemine çakma; zemin türüne göre izin: kayada red), yatay ray (iki kazık arası), çapraz destek. *Kabul:* iki kazık arasına ray konmadan çit paneli oluşmaz.
- [ ] **SOTF-K05 (P0)** — Çit ve savunma hattı: kazık + ray; sivri kazıklı savunma hattı temasla hasar verir ve kendi dayanıklılığını kaybeder. *Kabul:* baskın AI sivri hattı yol maliyetine ekler (Bölüm 2).
- [ ] **SOTF-K06 (P1)** — Örgü panel (wattle): kazıklar arasına yaş ince çubuk örülür; tek başına rüzgârı kısmen keser; kil sıva ile (Bölüm 7) tam duvar olur. *Kabul:* sıvasız örgü odası barınak puanı düşük; sıvalı yüksek.
- [ ] **SOTF-K07 (P1)** — Çatı iskeleti: ince çubuk/sırık kirişler + saman, kabuk veya yaprak kaplama. *Kabul:* kaplamasız iskelet yağmuru kesmez.
- [ ] **SOTF-K08 (P1)** — Üçayak ve çerçeve: halatla bağlanan çubuklar; kurutma rafı, tencere asacağı, çadır iskeleti.
- [ ] **SOTF-K09 (P1)** — Tuzak tetikleri: 4 figürü (çentikli 3 çubuk), ip kementi, düşen kütük tuzağı tetiği. *Mevcut:* `EPrimitiveTrapState`. *Kabul:* tuzak ancak doğru çentikli parçalarla kurulur.
- [ ] **SOTF-K10 (P1)** — Çubuk üretim tarifleri (ayrı item): mızrak (sertleştirilmiş sivri), ok şaftı (soyulmuş, düz, kuru) + ok ucu (çakmaktaşı) + tüy, yay (esnek tür, kuru, kirişli), meşale (çubuk + reçine/yağ + bez), alet sapı.
- [ ] **SOTF-K11 (P1)** — Yakıt rolü: çıra (ince kuru çubuk) ateş başlatmada zorunlu; çubuk kısa yanar, kütük uzun yanar (Bölüm 6).
- [ ] **SOTF-K12 (P2)** — Esnek filiz kaynakları (söğüt vb.) ve sepet örme (depolama kabı).

### 1.11 Görevler — taş

- [ ] **SOTF-S01 (P0)** — Taş veri yapısı: boyut bandı, tür, şekil, kütle, durum. *Kabul:* iki elle taşınan granit el taşından ağır; tablo.
- [ ] **SOTF-S02 (P0)** — Taşıma: çakıl kap içinde stok; el taşı tek elde (2 adet); iki elle taşınan tek ve yürüme yavaş; kaya taşınamaz (kaldıraç P2). Fırlatma: el taşı fırlatılabilir, hasar kütleyle orantılı. *Kabul:* kaya alma reddedilir.
- [ ] **SOTF-S03 (P0)** — Kırma ve yontma eylemleri (1.5 tablosu); çakmaktaşı yonga çıkarma ayrı eylem ailesi. *Kabul:* granitte yonga çıkarma reddedilir.
- [ ] **SOTF-S04 (P0)** — Kuru duvar kuralı: taş, oturma yüzeyindeki temas noktalarına göre yerleşir; derz kaydırma (iki alt taşa basma) bağlantı gücünü artırır. *Kabul:* üst üste hizalı dizilen taş kolonu, derzli duvardan daha düşük yükte çöker.
- [ ] **SOTF-S05 (P0)** — Taş zemin, yol ve basamak: rota maliyetini düşürür, çamur etkisini kaldırır (Bölüm 3). *Kabul:* taş yol döşenen kenarda yağmurlu havada maliyet artmaz.
- [ ] **SOTF-S06 (P0)** — Taş ocak/şömine: ısı kaynağının rüzgâr ve yağmur korumasını artırır (Bölüm 6). *Kabul:* taş ocaklı ateş rüzgârda sönmez.
- [ ] **SOTF-S07 (P1)** — Harçlı duvar: kireç harcı tüketir; kuruma süresi; bağlantı gücü artışı. *Bağımlı:* Bölüm 7 kireç zinciri.
- [ ] **SOTF-S08 (P1)** — Taş temel: üstüne kütük duvar; kütük çürüme hızını düşürür; arazi revizyonu değişirse temel desteği yeniden değerlendirilir.
- [ ] **SOTF-S09 (P1)** — İstinat duvarı ve set: arazi ve su ile etkileşen taş yapıları (Bölüm 2 ve 3). *Kabul:* kazı kenarına istinat duvarı konunca o kenarda toprak kayması olmaz.
- [ ] **SOTF-S10 (P1)** — Taş tutucu ve çakıl kabı depolaması; görsel doluluk.
- [ ] **SOTF-S11 (P2)** — Kemer ve lento: açıklık üstünde taş kemer; kemer taşı sırası tamamlanmadan destek gerektirir.

### 1.12 Görevler — taşıma, serbest inşa, destek, yardımcı

- [ ] **SOTF-C01 (P0)** — Taşıma yuvaları: omuzda 2 kütük, demet çubuk, elde taş (veri tablosu); kütle hız ve stamina etkiler. *Mevcut:* `ULogCarryComponent` (yalnız kütük). *Kabul:* aynı bileşen üç malzemeyi taşır.
- [ ] **SOTF-C02 (P1)** — Kızak: kütük, çubuk demeti ve taş yuvaları; eğimde yavaşlar; devrilince yük dökülür, her item tek sahipli.
- [ ] **SOTF-C03 (P1)** — Malzemeye özel tutucular: kütük, çubuk, taş tutucu; yalnız kendi malzemesini kabul eder.
- [ ] **SOTF-B01 (P0)** — Yerleştirme modları (yatay, dikey, çapraz) ve zemin/destek kuralı. *Mevcut:* `ELogPlacementMode`.
- [ ] **SOTF-B02 (P0)** — Duvar sırası mantığı ve "assembly" kimliği.
- [ ] **SOTF-B03 (P0)** — Önizleme = commit; red sebebi kodu ve UI metni.
- [ ] **SOTF-B04 (P1)** — Zemin döşeme ve açıklık limiti.
- [ ] **SOTF-B05 (P1)** — Çatı ve barınak bölgesi algılama (Bölüm 6 girdisi).
- [ ] **SOTF-B06 (P1)** — Merdiven ve rampa (navmesh ile).
- [ ] **SOTF-B07 (P1)** — Kapı/kapak aktörü; AI için kırılabilir hedef.
- [ ] **SOTF-B08 (P1)** — Hazır plan hayaleti; yarım plan kayıtta korunur; iptalde malzeme iadesi.
- [ ] **SOTF-D01 (P0)** — Çoklu destek (tek `ParentAnchorId` yerine edge listesi).
- [ ] **SOTF-D02 (P0)** — Çökme fiziği; çöken parça toplanabilir item.
- [ ] **SOTF-D03 (P1)** — Söküm ve bağımlı çökme önizlemesi.
- [ ] **SOTF-A01 (P1)** — Yardımcı komutları: kütük/çubuk/taş topla, yapıyı tamamla, alanı temizle; aynı sahiplik API'si.

---

## 2. 7 Days to Die — smooth voxel kazılabilir arazi ve baskınlar

Blok/küp inşa, şekil menüsü ve blok yükseltme sistemi **alınmaz**; oyuncunun yapıları SOTF tipi fiziksel parçalardır. 7DtD'den alınan iki çekirdek: (1) dünyanın pürüzsüz ama kazılabilir arazisi, (2) düzenli ve büyüyen baskın baskısı.

### 2.1 Mekanik ayrıştırması

**Arazi:** 7DtD'nin arazisi yoğunluk (density) taşıyan voxel'lerden pürüzsüz yüzey olarak üretilir; toprak, çakıl, kum, taş, cevher gibi malzemeler kazılabilir; kazılan voxel malzeme item'ına dönüşür; yer altında tüneller ve odalar açılabilir; düşmanlar da kazabilir.

**Baskın:** Varsayılan her 7. gece "Kan Ayı" dalgası; dalga büyüklüğü ve düşman türleri gün sayısı ve oyuncu ilerlemesiyle artar. Düşmanlar oyuncuya giden yolu, kırılacak engellerin sağlığını da maliyete katarak seçer; en zayıf noktaya yüklenir; kazarak ya da üst üste binerek ilerleyebilir. Gündüz ve gece arasında düşman davranışı değişir. Faaliyet "ısısı" (gürültü, ateş, üretim) gözcü düşmanları çeker.

### 2.2 Görevler — arazi mimarisi kararı

- [ ] **7D-V01 (P0)** — Mimari karar kaydı: (a) tüm dünya smooth voxel, (b) heightfield Landscape + seçilmiş bölgelerde smooth voxel, (c) tüm dünya smooth voxel ama uzak mesafede düşük çözünürlük. Kriterler: dünya boyutu, kazı derinliği, baskın AI'ın kazması, performans bütçesi, kayıt boyutu, UE 5.8 araçları. *Kabul:* seçim, ölçülmüş bir prototip performans sonucu ile belgelenir; karar verilmeden TERR görevleri başlamaz.
- [ ] **7D-V02 (P0)** — Hazır eklenti mi kendi çekirdeğimiz mi: Voxel Plugin vb. ile kendi Marching Cubes / Surface Nets / Dual Contouring uygulamasını; lisans, runtime düzenleme olgunluğu, UE 5.8 uyumu, kayıt formatı denetimi ve kaynak koda erişim kriterleriyle karşılaştır. *Kabul:* yazılı karşılaştırma + 1 günlük prototip.

### 2.3 Görevler — smooth voxel çekirdeği

- [ ] **7D-V03 (P0)** — Veri modeli: chunk başına yoğunluk (SDF, işaretli mesafe) + malzeme ID'si; chunk boyutu, voxel boyutu (ör. 25–50 cm), sınır dolgusu (komşu örnekler). *Mevcut:* `WorldTerrainStateSubsystem` hücre modeli (`Cell`, `DepthLayer`, `MaterialId`, `bExcavated`) — yoğunluk yok, ikili kazılmış/kazılmamış. *Kabul:* yoğunluk ara değerleri yüzeyi pürüzsüz yapar; ikili modelle küp görünümü oluşmaz.
- [ ] **7D-V04 (P0)** — Yüzey çıkarma: seçilen algoritma ile chunk mesh'i; normal hesaplama; malzeme karışımı (vertex rengi veya ağırlık); komşu chunk sınırında dikiş yok. *Kabul:* iki chunk sınırında kazılan çukurun kenarında çatlak görünmez.
- [ ] **7D-V05 (P0)** — Collision: değişen chunk'ın collision'ı aynı commit'te güncellenir; karakter kazılan alana düşer. *Kabul:* kazıdan sonraki ilk fizik adımında karakter yeni yüzeye basar.
- [ ] **7D-V06 (P0)** — Navmesh: değişen bölge sınırlarıyla yerel yeniden oluşturma; AI yeni tüneli kullanır, kapanan yolu bırakır. *Kabul:* kazılan tünel 2 sn içinde AI rotasına girer.
- [ ] **7D-V07 (P0)** — Kazı fırçaları: küre, silindir, düzleştirme, yumuşatma, doldurma; yarıçap ve güç aletle sınırlı. *Kabul:* kürek küçük yarıçap/yumuşak malzeme; kazma taş; düzleştirme yalnız toprak/çakılda.
- [ ] **7D-V08 (P0)** — Hacim → item: kazılan yoğunluk farkı × malzeme → item miktarı; tek işlemde (arazi + item). *Kabul:* aynı OperationId tekrarında item çoğalmaz; dolu envanterde kazı reddedilir veya item yere düşer (kural seçilip testlenir).
- [ ] **7D-V09 (P0)** — Doldurma: taşınan malzeme item'ı tüketilerek hacim eklenir; malzeme kimliği korunur.
- [ ] **7D-V10 (P0)** — Kalıcılık: seed'den üretilen temel arazi + yalnız değişen chunk delta'ları kaydedilir; yükleme sırası arazi → collision → navmesh → aktörler. *Kabul:* kazılmış 100 chunk'lık dünya kaydı, değişmemiş chunk'ları içermez; yükleme sonrası aynı yüzey.
- [ ] **7D-V11 (P1)** — Toprak kayması: gevşek malzeme (kum, toprak, çakıl) duruş açısını (angle of repose) aşınca yoğunluğu aşağı akar; taş akmaz. *Kabul:* dik kum duvarı birkaç adımda eğime yayılır; taş duvar dik kalır.
- [ ] **7D-V12 (P1)** — Tavan çökmesi: yer altı boşluğunda tavan açıklığı malzeme limitini aşarsa ve destek (kütük direk, taş ayak) yoksa tavan voxel'leri düşer. *Bağımlı:* SOTF-D01 terrain-support düğümü. *Kabul:* destekli tünel ayakta; desteksiz geniş oda çöker.
- [ ] **7D-V13 (P1)** — Malzeme tabakaları ve cevher: seed + derinlik + bölge ile deterministik tabaka ve damar; yüzeyde iz. *Kabul:* aynı seed aynı damarlar.
- [ ] **7D-V14 (P1)** — Su ile etkileşim: kazılan çukur yeraltı su seviyesinin altına inerse dolar; yağmurda çukurda su birikir (Bölüm 3). *Kabul:* nehre bağlanan hendek suyla dolar.
- [ ] **7D-V15 (P1)** — Yapı desteği: yapının oturduğu voxel kazılırsa graph'taki zemin desteği düşer. *Kabul:* temelin altı kazılınca temel ve bağlı duvar çöker veya kazı önizlemesi uyarı verir.
- [ ] **7D-V16 (P2)** — LOD ve uzak chunk optimizasyonu, asenkron mesh üretimi, eski işlerin iptali, bellek bütçesi.

### 2.4 Görevler — baskın döngüsü

- [ ] **7D-R01 (P0)** — Baskın takvimi: her N günde (veri; 7DtD varsayılanı 7), uyarı belirtileri (gökyüzü, ses, hayvan kaçışı), uyarı süresi, dalga sayısı, dalga bütçesi, gün sayısı ve ilerlemeyle artış. *Mevcut:* `WorldThreatDirectorSubsystem`. *Kabul:* aynı tick yeniden işlenince ikinci baskın açılmaz; kayıt sonrası takvim korunur.
- [ ] **7D-R02 (P0)** — Ara dönem tehditleri: gezgin gruplar, gözcüler, gece daha agresif davranış. *Kabul:* baskın dışı günlerde de düşük yoğunlukta tehdit var.
- [ ] **7D-R03 (P0)** — Faaliyet ısısı: ateş, kesim, kazı, üretim, gürültü bölgesel ısı ızgarasına puan ekler; sönüm; eşik aşılınca gözcü gelir. *Kabul:* çok ocaklı üs ızgarası ısınır; faaliyet bitince söner; kayıtta tutulur.
- [ ] **7D-R04 (P0)** — Yol maliyeti = mesafe + engel sağlığı × kırma katsayısı + tuzak hasarı × korku katsayısı. AI kırarak geçmeyi de hesaplar. *Kabul:* demir kapı + çürük kütük yan duvar → AI yan duvara yönelir.
- [ ] **7D-R05 (P0)** — Kırma: yolu kapatan parçaya saldırı, hasar `ApplyDamage` ile; parça düşünce yol yeniden hesaplanır. *Mevcut:* `WorldRaidAdapterSubsystem` Pawn spawn + periyodik hasar. *Kabul:* duvar kırılınca AI 1 sn içinde devam eder.
- [ ] **7D-R06 (P0)** — Kazan düşman: smooth voxel'i kazarak duvarın altından geçer; kazı aynı arazi API'si ile (item üretmez). *Bağımlı:* 7D-V07. *Kabul:* yüzeyden erişilemeyen üse kazıcı tünel açar; tünel kayda girer.
- [ ] **7D-R07 (P1)** — Yaklaşım yönü ve doğuş bölgesi: navigasyonlu zemin, oyuncuya mesafe, görüş dışı, arazi engelleri; nehir ve dik yamaç yaklaşımı yönlendirir (Bölüm 3). *Kabul:* tek köprüyle bağlı adaya baskın köprüden gelir; köprü yıkılınca yüzme/bekleme/başka yol davranışı tanımlı.
- [ ] **7D-R08 (P1)** — Düşman türleri (veri): yürüyen, koşan (gece), kırıcı (yapıya yüksek hasar), kazıcı, tırmanan (çit/düşük duvar). *Kabul:* her tür aynı AI katmanında farklı maliyet ağırlıkları kullanır.
- [ ] **7D-R09 (P1)** — Tuzaklar ve savunma: sivri kazık hattı (SOTF-K05), hendek (kazı ile; düşmeyi yavaşlatır), düşen kütük tuzağı, ateş hendeği (P2). *Kabul:* hendek AI yol maliyetini artırır; içine düşen düşman tırmanma süresi kaybeder.
- [ ] **7D-R10 (P1)** — Baskın sonrası: hasarlı parçalar, düşman kalıntıları (post/kemik → Bölüm 7), onarım ihtiyacı listesi. *Kabul:* baskın sonrası özet: kırılan parça sayısı, hasar, ganimet.
- [ ] **7D-R11 (P1)** — Zorluk eğrisi: dalga bütçesi = taban + gün × artış + üs değeri katsayısı; üst sınır. *Kabul:* tablo değişince bütçe değişir; aynı gün aynı seed aynı bütçe.

### 2.5 Görevler — oyuncu yapılarının yapısal bütünlüğü (blok değil, fiziksel parça)

- [ ] **7D-S01 (P0)** — Malzeme başına yük profili (kütük, çubuk, taş, harçlı taş): kütle, taşıma, konsol uzanma.
- [ ] **7D-S02 (P0)** — Toplam yük hesabı (çoklu destekte paylaştırma). *Bağımlı:* SOTF-D01.
- [ ] **7D-S03 (P0)** — Deterministik zincirleme çökme.
- [ ] **7D-S04 (P1)** — Yük görünümü (debug modu).
- [ ] **7D-S05 (P1)** — Onarım: hasarlı parça kendi malzemesiyle onarılır (blok yükseltme yok).

---

## 3. Death Stranding — yaşayan dünya ve kalıcı değişim

### 3.1 Mekanik ayrıştırması

Death Stranding'in dünyası oyuncuya karşı **aktif** bir engeldir: zaman yağmuru, dik yamaçlar, derin ve akıntılı nehirler, kayalık alanlar. Oyuncu geçemediği yere çözüm üretir: merdiven, halat, köprü, yol. Bir kez kurulan altyapı o bölgeyi **kalıcı olarak** değiştirir; aynı yolun ikinci geçişi çok daha kolaydır. Ancak dünya da yapıları aşındırır; bakım gerekir. Oyuncu rota planlar, yükü dengeler, kargonun durumunu korur.

MyProject3 için çekirdek: **doğa kendi kendine değişir, oyuncu dünyayı kalıcı değiştirir, iki değişim birbirini etkiler.**

### 3.2 Yaşayan dünya olayları

| Olay | Tetikleyici | Dünyaya etkisi | Oyuncu çözümü |
|---|---|---|---|
| Yağmur | Hava sistemi | Zemin çamurlaşır, nehir seviyesi yükselir, ateş söner, giysi ıslanır | Taş yol, çatı, taş ocak |
| Sağanak ve sel | Yağış birikimi | Nehir taşar, alçak alan su basar, köprü hasar alır, kargo sürüklenir | Yüksek köprü, set, hendek |
| Kar ve don | Mevsim/sıcaklık | Nehir donar (geçilebilir ama kırılabilir), yol kapanır, tarla işlenemez | Kızak, buz kalınlığı kontrolü |
| Kar erimesi | İlkbahar | Nehir debisi en yüksek, sığ geçitler kaybolur | Köprü, sal |
| Kuraklık | Uzun yağışsızlık | Nehir sığlaşır (geçit açılır), tarla kurur | Sulama, kuyu |
| Fırtına | Hava olayı | Ağaçlar devrilir, yolları kapatır, zayıf yapılar hasar alır | Kesim, sağlam yapı |
| Toprak kayması | Yağmur + eğim + kazı | Yamaç voxel'leri akar, yolu ve yapıyı örter | İstinat duvarı, drenaj hendeği |
| Erozyon (P2) | Sürekli su akışı | Nehir yatağı ve hendek yavaşça derinleşir | Taş kaplama |

### 3.3 Görevler — su ve nehir

- [ ] **DS-W01 (P0)** — Nehir veri modeli: spline veya hücre ızgarası üzerinde su yüzeyi yüksekliği, derinlik, akıntı hızı ve yönü; mevsim ve yağışla değişen debi. *Kabul:* aynı noktada yaz ve ilkbahar derinliği farklı; değer kayıtta tutulur.
- [ ] **DS-W02 (P0)** — Geçilebilirlik kuralı: derinlik ve akıntıya göre dört sınıf: kuru, sığ (yavaşlatır), orta (dengeyi zorlar, ağır yük dökülür), geçilemez (sürükler). *Kabul:* ilkbaharda geçilemez olan geçit, yaz kuraklığında sığ sınıfa iner.
- [ ] **DS-W03 (P0)** — Suda fizik: suya düşen kütük yüzer ve akıntıyla sürüklenir; taş batar; karakter derin suda yükünü kaybeder. *Kabul:* sürüklenen kütük kıyıya takılınca durur ve sahipliği `World` olur.
- [ ] **DS-W04 (P0)** — Yağış birikimi: hava sisteminden gelen yağış miktarı havza bazında nehir seviyesini gecikmeli yükseltir (yağmur bitince de bir süre yükselmeye devam eder). *Mevcut:* `EWorldWeatherState {Clear, Rain, Wind, Snow, Storm, Heatwave}` — miktar ve havza yok. *Kabul:* 2 gün yağmurdan sonra nehir tepe seviyesine yağmur bittikten sonra ulaşır.
- [ ] **DS-W05 (P1)** — Sel: seviye kıyı yüksekliğini aşınca komşu alçak hücrelere su yayılır; içindeki gevşek item'lar sürüklenir, tarla hasar alır, ateş söner. *Kabul:* nehir kenarındaki kamp sel sırasında su altında kalır; yüksekteki kamp etkilenmez.
- [ ] **DS-W06 (P1)** — Donma: sıcaklık eşiğin altında nehir buz tutar; buz kalınlığı zamanla artar; ağır yükle ince buzda kırılma riski. *Kabul:* ince buzda iki kütükle yürümek kırılma olasılığını tabloya göre artırır.
- [ ] **DS-W07 (P1)** — Su ile kazı etkileşimi: nehre bağlanan hendek suyla dolar; set (taş/toprak) suyu tutar. *Bağımlı:* 7D-V14, SOTF-S09. *Kabul:* set ile kesilen kol kurur; set yıkılınca su geri akar.

### 3.4 Görevler — geçiş çözümleri ve kalıcı değişim

- [ ] **DS-X01 (P0)** — Basamak taşları: sığ ve orta sularda büyük taşlar (iki elle taşınan) yerleştirilerek geçit oluşturulur; taşlar selde yerinden oynayabilir. *Kabul:* basamak taşı dizilince o noktada geçiş sınıfı "kuru yol"a iner; sel sonrası bir taş kaymış olabilir.
- [ ] **DS-X02 (P0)** — Kütük köprü: iki kıyıda zemin/taş ayak desteği; açıklık limiti kütük uzunluğu ve çap bandına bağlı; plank döşeme. Rota grafiğine kalıcı kenar ekler. *Bağımlı:* SOTF-D01, SOTF-B04. *Kabul:* köprü kurulunca rota uzunluğu ve süresi düşer; save/load sonrası köprü ve kenar korunur.
- [ ] **DS-X03 (P0)** — Köprü yüksekliği ve sel: köprü tabliyesi sel seviyesinin altındaysa selde hasar alır veya sürüklenir; taş ayaklı yüksek köprü dayanır. *Kabul:* alçak köprü büyük selde yıkılır ve kütükleri akıntıya kapılır.
- [ ] **DS-X04 (P1)** — Taşınabilir merdiven ve halat: geçici köprü/tırmanış; geri alınabilir. *Kabul:* alınan merdivenle kenar silinir.
- [ ] **DS-X05 (P1)** — Sal ve feribot halatı: kütük + halat sal; iki kıyı arası gerilen halatla akıntıda kontrollü geçiş. *Kabul:* halatsız sal akıntıyla sürüklenir.
- [ ] **DS-X06 (P1)** — Yol yapımı: patika (yürüdükçe oluşan iz) → çakıl yol → taş yol. Kullanılan güzergâh zamanla iz oluşturur (maliyet düşer), taş yol çamurlanmaz. *Kabul:* 20 kez geçilen güzergâh iz olarak görünür ve rota maliyeti düşer.
- [ ] **DS-X07 (P1)** — Ağaç devirerek köprü: devrilme yönü kontrolüyle dar dereye ağaç yatırılır (SOTF-T02). *Kabul:* dereye devrilen ağaç geçilebilir kenar oluşturur.
- [ ] **DS-X08 (P1)** — Zipline: kargo için iki direk arası kablo.
- [ ] **DS-X09 (P1)** — Aşınma ve bakım: yapılar yağmur, sel, kar ile sağlık kaybeder; bakım malzeme ister. *Kabul:* bakımsız ip köprü N günde kullanılmaz olur.

### 3.5 Görevler — kalıcı değişimin dünyaya yayılması

- [ ] **DS-P01 (P0)** — Tek rota grafiği: oyuncu, yardımcı, kargo, baskın AI ve hayvanlar aynı grafiği kullanır; köprü/yol/set/kazı grafiği günceller. *Mevcut:* `WorldLogisticsRouteSubsystem` (`FLogisticsRouteEdge` FName düğümlü, sabit `Cost`). *Kabul:* köprü kurulunca baskın AI de köprüyü kullanır; köprü yıkılınca herkes için kenar kalkar.
- [ ] **DS-P02 (P0)** — `RouteCostProvider`: maliyet = mesafe × eğim × yüzey (çamur/kar/taş yol/iz) × su sınıfı × yük × hava. *Kabul:* aynı kenar yağmurda pahalı, taş yol döşenince yağmurda da ucuz.
- [ ] **DS-P03 (P1)** — Dünya değişim günlüğü: oyuncunun ve doğanın yaptığı kalıcı değişiklikler (köprü kuruldu, sel köprüyü yıktı, toprak kayması yolu kapattı) zaman damgasıyla; harita ve defterde görüntülenir. *Kabul:* oyuncu "bu köprü ne zaman yıkıldı" bilgisini defterden görebilir.
- [ ] **DS-P04 (P2)** — Hayvanlar ve düşmanlar oyuncu yollarını öğrenir: sık kullanılan yollar avcı ve baskın için de çekici olur.

### 3.6 Görevler — kargo ve denge

- [ ] **DS-C01 (P0)** — Item kütle/hacim/yuva tipi; bilinmeyen kütle red.
- [ ] **DS-C02 (P0)** — Kapasite ve aşırı yük kademeleri.
- [ ] **DS-C03 (P1)** — Ağırlık merkezi ve denge: eğim + koşu + dengesiz yük → sendeleme → düşme ve yükün dökülmesi.
- [ ] **DS-C04 (P1)** — Kap dayanıklılığı (hassas yük: çömlek, yumurta, tohum).
- [ ] **DS-C05 (P1)** — Arazi tarama görünümü: eğim, su derinliği ve geçiş sınıfı renklerle.

---

## 4. Farming Simulator — tarım ve hayvancılık derinliği

### 4.1 Mekanik ayrıştırması

**Tarım:** Toprak işleme (sürme/kültivatör), taş toplama, ekim, gübreleme (birden çok kademe), kireçleme (pH), yabancı ot mücadelesi, merdane, hasat, anız/malç. Her işlem tarla yüzeyindeki bir durum katmanını değiştirir. Ürünlerin ekim/hasat ayları ve büyüme takvimi vardır; ekim nöbeti, sürme bonusu, ıslak hasat, don. Hasat ana ürün + yan ürün (saman) verir; silo/ambar, balya, silaj.

**Hayvancılık:** Türe göre yem karışımı, su, altlık, temizlik; ürünler (süt, yün, yumurta, gübre); üreme; sağlık ve verimlilik; ahır kapasitesi; mera.

**Ekonomi ve üretim:** Değirmen, fırın, mandıra; mevsimsel fiyat.

### 4.2 Toprak ve ürün katmanları

| Hücre katmanı | Değer aralığı | Nasıl değişir | Etki |
|---|---|---|---|
| İşlenme | işlenmemiş / sürülmüş / tırmıklanmış / ekili / anız | Aletler | Ekim önkoşulu |
| Nem | 0–1 | Yağmur, sulama, buharlaşma (sıcaklık/rüzgâr), drenaj | Büyüme hızı, kuruma, çürüme |
| Azot | 0–3 kademe | Gübre, baklagil, hasat tüketir | Verim çarpanı |
| Fosfor/potas (P2) | 0–3 | Kül, kemik unu | Verim çarpanı |
| pH | asidik / nötr / bazik | Kireç, kül; zamanla asitlenir | Aralık dışı verim cezası |
| Organik madde | 0–1 | Kompost, gübre, malç | Nem tutma, uzun vadeli verim |
| Sıkışma | 0–1 | Üzerinde yürüme/taşıma, ıslakken çalışma | Kök gelişimi |
| Yabancı ot | yok / küçük / büyük | Zamanla, çapa | Verim rekabeti |
| Taş | 0–1 | Sürmede ortaya çıkar, toplanır | Alet aşınması |
| Hastalık/zararlı (P1) | yok / hafif / ağır | Nem, ekim nöbeti ihlali, mevsim | Verim, ürün kaybı |
| Ürün | tür, evre, sağlık, kalite | Büyüme | Hasat |

### 4.3 Görevler — tarla ve toprak

- [ ] **FS-F01 (P0)** — Parsel ve hücre ızgarası (ör. 1 m²) arazi üzerinde; kayıtta delta olarak. *Mevcut:* yalnız tekil `FWorldCropState`. *Kabul:* parsel ve hücre durumları save/load sonrası aynı.
- [ ] **FS-F02 (P0)** — 4.2 tablosundaki katmanlar (P0: işlenme, nem, azot, pH, yabancı ot, taş, ürün). *Kabul:* her katman ayrı API ile değişir, hücre revizyonu ortak.
- [ ] **FS-F03 (P0)** — Toprak işleme el aletleri: çapa (küçük alan), kazma (sert/taşlı), tırmık. Sürme taş ortaya çıkarır (taş item → SOTF taş sistemi). *Kabul:* sürülmemiş hücreye ekim reddedilir.
- [ ] **FS-F04 (P0)** — Ekim: tohum item tüketir, yoğunluk, mevsim kontrolü; yanlış mevsimde ekim ya reddedilir ya da büyümez (kural seçilip testlenir).
- [ ] **FS-F05 (P0)** — Su: yağmur nemi artırır, sıcak/rüzgâr buharlaştırır, kova ile sulama (su item'ı), oluk/kanal ile sulama (Bölüm 7, HYD). *Mevcut:* `SetCropMoisture`. *Kabul:* 3 gün kuraklık ürünü strese sokar; kanalla bağlı tarla etkilenmez.
- [ ] **FS-F06 (P1)** — Gübre kademeleri: hayvan gübresi, kompost, kül, kemik unu; her biri farklı katmanı etkiler. *Kabul:* kül pH'ı yükseltir ve potas verir; gübre azot verir.
- [ ] **FS-F07 (P1)** — Kireç ve pH: kireçtaşı → kireç (Bölüm 7) ile pH düzeltme; pH zamanla asitlenir.
- [ ] **FS-F08 (P1)** — Yabancı ot döngüsü ve çapalama.
- [ ] **FS-F09 (P1)** — Sıkışma: ıslak tarlada yürümek/taşımak sıkıştırır; sürme açar.
- [ ] **FS-F10 (P1)** — Ekim nöbeti ve nadas: aynı ürün üst üste ceza + hastalık riski; baklagil sonrası azot bonusu; nadasta organik madde artar.
- [ ] **FS-F11 (P1)** — Kompost: yaprak, saman, mutfak artığı, gübre → kompost yığını (süre, nem, çevirme) → gübre item'ı.
- [ ] **FS-F12 (P2)** — Drenaj hendeği: aşırı ıslak tarlada hendek (kazı) nemi düşürür (7D-V ile).

### 4.4 Görevler — ürünler

- [ ] **FS-G01 (P0)** — Ürün tanımı data asset: evre sayısı/süreleri, ekim ayları, hasat penceresi, nem ve azot ihtiyacı, don/sıcak toleransı, ana ve yan ürün verimi, tohum geri dönüşü, depolama ömrü. *Mevcut:* sabit `ECropGrowthStage {Seed, Sprout, Growing, Mature, Withered}`. *Kabul:* evre sayısı ürün tanımından gelir.
- [ ] **FS-G02 (P0)** — Mevsim takvimi shared clock'tan. *Kabul:* kışın ekilen buğday ilkbahara kadar büyümez.
- [ ] **FS-G03 (P0)** — Hasat: ana ürün + yan ürün item; hücre anıza geçer; tekrarda çoğalmaz.
- [ ] **FS-G04 (P0)** — Verim formülü: taban × azot × pH × nem geçmişi × ot rekabeti × sıkışma × hastalık × hasat zamanı. Her çarpan ayrı ayrı görüntülenebilir. *Kabul:* tarla raporu verimin hangi çarpandan düştüğünü gösterir.
- [ ] **FS-G05 (P1)** — Hasat penceresi, ıslak hasat ve kurutma; kaçırılırsa kalite düşüşü.
- [ ] **FS-G06 (P1)** — Don, kar, sıcak dalgası etkileri (Bölüm 3 ve 6).
- [ ] **FS-G07 (P1)** — Ürün seti (ağ için seçilmiş): buğday (un, saman), arpa (yem), patates (doğrudan yemek, uzun depolama), keten (lif → ip/bez, tohum → yağ), fasulye/bezelye (azot bağlama, kuru depolama), lahana (kış ürünü), ot/çayır (hayvan yemi, kuru ot).
- [ ] **FS-G08 (P1)** — Tohum saklama: hasattan tohum ayırma kararı (yemek mi tohum mu); tohum kalitesi gelecek verimi etkiler.
- [ ] **FS-G09 (P1)** — Zararlı ve hastalık: kuş (korkuluk), mantar (nem + nöbet ihlali), kemirgen (depoda). *Kabul:* korkuluksuz olgun tarla kuş kaybı yaşar.
- [ ] **FS-G10 (P2)** — Meyve ağaçları ve çok yıllık bitkiler: yıllar içinde olgunlaşan, budama isteyen.

### 4.5 Görevler — depolama ve işleme

- [ ] **FS-P01 (P0)** — Ambar ve kiler: dökme ürün stok; nem ve sıcaklık bozulmayı etkiler; kemirgen riski. *Mevcut:* `WorldStorageSubsystem`.
- [ ] **FS-P02 (P1)** — Kuru ot ve saman: biçme → kurutma (hava durumuna bağlı) → demet/yığın; ıslanırsa çürür.
- [ ] **FS-P03 (P1)** — Harman: tahıl → dövme/savurma → tane + saman + kepek.
- [ ] **FS-P04 (P1)** — Değirmen: el değirmeni → su değirmeni (Bölüm 7).
- [ ] **FS-P05 (P2)** — Silaj ve fermantasyon; turşu/konserve (çömlek, tuz).

### 4.6 Hayvancılık

| Tür | Yem | Ürün | Yan ürün | Özel ihtiyaç |
|---|---|---|---|---|
| Tavuk | Tahıl, artık, böcek (serbest dolaşımda) | Yumurta, et | Tüy (ok), gübre (yüksek azot) | Kümes, yırtıcı koruması, gece kapama |
| Keçi | Ot, çalı, kuru ot | Süt, et | Post, kemik, gübre | Tırmanma, çiti zorlar |
| Koyun | Ot, kuru ot | Yün, süt, et | Post, kemik, gübre | Yün kırkma mevsimi |
| İnek (P1) | Ot, kuru ot, tahıl | Süt, et | Deri, kemik, gübre, iş gücü (çekiş) | Bol su, büyük ahır |
| Domuz (P2) | Artık, tahıl, palamut | Et, yağ | Deri, kemik | Çamur, güçlü çit |
| Arı (P2) | Çiçek alanı | Bal, balmumu | — | Kovan, mevsim |

### 4.7 Görevler — hayvancılık

- [ ] **FS-A01 (P0)** — Tür tanımı data asset: yem tercihleri ve günlük miktar, su, barınak/alan ihtiyacı, sıcaklık toleransı, ürün ve üretim aralığı, olgunlaşma süresi, üreme koşulları, yaşam süresi. *Mevcut:* `FWorldAnimalState`, `FeedAnimal(AnimalId, HungerReduction, ExpectedRevision)` (item tüketmiyor). *Kabul:* yeni tür yalnız veriyle eklenir.
- [ ] **FS-A02 (P0)** — Yemlik ve suluk: fiziksel aktör + depolama; hayvan yemlikten tüketir; kütük yalak (`TroughLog`) suluk olarak çalışır. *Kabul:* yemlik boşsa açlık artar; yalakta su varsa susuzluk giderilir.
- [ ] **FS-A03 (P0)** — Ürün item olarak doğar (yumurta, süt kova ile sağım, yün kırkma); toplanmayan ürün bozulur. *Kabul:* sağılmayan keçinin süt verimi düşer.
- [ ] **FS-A04 (P0)** — İlk tür tavuk + ikinci tür keçi; ikisi farklı yem ve barınak ister.
- [ ] **FS-A05 (P1)** — Mera ve otlatma: çitle çevrili alanda ot hücreleri tüketilir ve yenilenir; aşırı otlatma meranın toparlanmasını yavaşlatır. *Bağımlı:* FS-F01 hücre modeli. *Kabul:* küçük merada fazla keçi otu bitirir; kuru ot takviyesi gerekir.
- [ ] **FS-A06 (P1)** — Barınak, altlık ve temizlik: altlık (saman) değişmezse temizlik düşer, hastalık riski artar; kışın barınak sıcaklığı verimi etkiler.
- [ ] **FS-A07 (P1)** — Sağlık ve hastalık: kötü bakım, ıslaklık, soğuk; tedavi (bitki, ayırma); bulaşma. *Kabul:* hasta hayvan ayrılmazsa komşu hayvana bulaşma olasılığı.
- [ ] **FS-A08 (P1)** — Üreme ve yavru: koşullar sağlanınca gebelik süresi, yavru doğumu, yavrunun büyümesi; kapasite doluysa üreme durur. *Kabul:* yavru olgunlaşınca ürün vermeye başlar.
- [ ] **FS-A09 (P1)** — Kesim ve karkas: et, post, kemik, yağ, bağırsak (Bölüm 7 ve TLD karkas sistemi). *Kabul:* kesim kararı ürün akışını kalıcı keser.
- [ ] **FS-A10 (P1)** — Yırtıcı baskısı: tilki/kurt geceleri kümese saldırır; çit ve kapı kontrolü (Bölüm 2 AI). *Kabul:* gece açık kalan kümes kayıp verir.
- [ ] **FS-A11 (P1)** — Hayvan davranışı: navmesh ile dolaşma, yemliğe gitme, sürü, kaçma; kırık çitten kaçış.
- [ ] **FS-A12 (P1)** — Gübre döngüsü: barınak gübresi toplanır → kompost/tarla (FS-F06).
- [ ] **FS-A13 (P2)** — Çekiş gücü: inek/öküz sapan ve kızak çeker (SF güç ağına mekanik güç).
- [ ] **FS-A14 (P2)** — Basit soy özellikleri: verim/sağlamlık ebeveynlerden kalıtılır (küçük varyasyon).

---

## 5. Subnautica — birkaç not, serbest keşif, isteyene derin araştırma

### 5.1 Mekanik ayrıştırması

Subnautica oyuncuya zorunlu bir görev yolu dayatmaz. Hikâye; bırakılmış kayıtlar, ölmüş kişilerin eşyaları, yıkıntılar, kısa notlar ve çevrenin kendisi (neyin neden bozulduğu) ile anlatılır. Oyuncu hayatta kalmakla uğraşırken hikâyeyi görmezden gelebilir; merak eden ise ipuçlarını birleştirerek derine iner. Keşfedilen bilgi pratik fayda da sağlar (yeni tarif, kaynak konumu, tehlike uyarısı). Bölgeler derinlik/ekipman ile doğal olarak kademelenir; anlatı keşif sırasını zorlamaz.

### 5.2 Anlatı yazım kuralları (içerik ekibi için)

1. Toplam not sayısı az tutulur (ilk sürüm için 15–25). Her not bir yere ve bir nesneye bağlıdır.
2. Notlar doğrudan anlatmaz; bir olayın bir parçasını gösterir. Bütün hikâye hiçbir notta yazmaz.
3. Her not en az bir **pratik** bilgi taşır (kil yatağının yeri, bir bitkinin zehirli olduğu, nehrin ilkbaharda taştığı).
4. Çevre de anlatır: yıkılmış köprü, terk edilmiş tarla, yarım kalmış set, mezarlar, yanmış kulübe.
5. Hiçbir not keşfedilmeden oyunun bir sistemi kilitli kalmaz (anlatı isteğe bağlıdır); yalnız kolaylık sağlar.
6. Notların sırası değişebilir; her biri tek başına anlamlıdır.

### 5.3 Görevler

- [ ] **SN-N01 (P0)** — Keşif noktası aktörü: kalıcı ID, tür (not, nesne, yıkıntı, mezar, kamp), içerik anahtarı, pratik fayda (tarif ipucu, harita işareti, tehlike bilgisi). *Mevcut:* `RecordDiscovery(FName, FVector, double)` — içerik ve fayda yok. *Kabul:* ikinci etkileşim yeni kayıt veya fayda üretmez; kayıt sonrası korunur.
- [ ] **SN-N02 (P0)** — Defter: okunan notlar, keşfedilen yerler ve öğrenilen pratik bilgiler; notlar konuya göre gruplanır ama sıra zorlanmaz. *Kabul:* not okunmadan da tüm sistemler kullanılabilir (anlatıya bağlı kilit yok — otomatik test).
- [ ] **SN-N03 (P0)** — Çevresel anlatı yerleşim seti (ilk sürüm 6–8 sahne): yıkılmış köprü + nehir notu (DS bağlantısı), terk edilmiş tarla + toprak notu (FS), yarım kalmış set, sel izi, baskın yaşamış yanık kulübe (7D), donmuş kamp (TLD), kazı ocağı (HYD). *Kabul:* her sahne en az bir pratik bilgi ve bir hikâye parçası taşır.
- [ ] **SN-N04 (P1)** — İnceleme eylemi: nesneye bakınca kısa gözlem metni (ör. "Kütüğün ucu yanık; ateş içeriden başlamış."). Ödül yok, yalnız bağlam. *Kabul:* her sahnede en az 3 incelenebilir nesne.
- [ ] **SN-N05 (P1)** — Araştırma zinciri (isteğe bağlı): belirli notların birleştirilmesiyle ortaya çıkan gizli konum veya bilgi; oyuncu kendisi çıkarım yapar, işaret verilmez. *Kabul:* zincir tamamlanmadan konum haritada gösterilmez; zincir zorunlu değildir.
- [ ] **SN-N06 (P1)** — Pratik bilgi ile tarif ipucu: not, tarifi doğrudan açmaz; malzemeyi ve yöntemi ima eder. Oyuncu deneyerek bulabilir; not yalnız kolaylaştırır. *Kabul:* not okumadan aynı tarif deneme ile keşfedilebilir.
- [ ] **SN-E01 (P1)** — Bölge kademeleri: uzak/zorlu bölgeler (dağ, mağara, nehir ötesi) daha iyi kaynak ve daha çok tehlike; ekipman ve hazırlık gerektirir, kilitle değil koşulla. *Kabul:* bölgeye ekipmansız girilebilir ama tehlike tabloya göre ölümcül.
- [ ] **SN-E02 (P1)** — Yer işareti: oyuncunun dünyaya koyduğu işaret (kazık + bez); HUD'da mesafe; harita olmadan yön bulma.
- [ ] **SN-E03 (P2)** — Kültür kalıntısı ile yeni yapı tekniği (ör. eski kemer köprü yıkıntısı incelenince taş kemer tarifi kolaylaşır).

---

## 6. Frostpunk + The Long Dark — hayatta kalmak için risk almak zorunda olma hissi

### 6.1 Mekanik ayrıştırması

İki oyun aynı duyguyu farklı ölçeklerde üretir:

- **The Long Dark (birey ölçeği):** Sıcaklık, yorgunluk, susuzluk, kalori ve genel sağlık sürekli azalır. Sığınakta kalmak güvenlidir ama yiyecek ve yakıt biter; dışarı çıkmak zorunludur. Hava değişir, gece çöker, rüzgâr ateşi söndürür, giysi ıslanır, kurtlar et kokusunu izler. Her çıkış "ne kadar uzağa gidebilirim, ne kadar yük taşıyabilirim, hava ne zaman bozulur" hesabıdır.
- **Frostpunk (topluluk ölçeği):** Sıcaklık takvime bağlı düşer, yakıt tüketimi artar, kaynaklar azalır; oyuncu insanları tehlikeli işlere, uzun vardiyalara, keşif seferlerine göndermek zorunda kalır. Her karar bir bedel öder.

Ortak ilke: **Güvenli durum kalıcı değildir; zaman ve tüketim oyuncuyu risk almaya iter. Atmosfer (ışık, ses, görüş, karakter tepkileri) bu baskıyı hissettirir.**

### 6.2 Risk zorunluluğunun ölçekleri

| Ölçek | Süre | Risk kaynağı | Oyuncu kararı | Gerekli sistemler |
|---|---|---|---|---|
| An | saniye | Düşme, buz kırılması, hayvan saldırısı, ateşin sönmesi, yük dökülmesi | Tepki | Fizik, AI, denge, ateş |
| Çıkış | saat | Hava penceresi, karanlık, yük sınırı, sıcaklık kaybı, mesafe | Ne kadar uzağa, ne kadar yükle | Hava tahmini, gün ışığı, kütle, ısı |
| Gün | 24 saat | Kalori, su, yakıt, uyku, ateşin gece boyu yanması | İş planı: kesim mi av mı onarım mı | Kalori, yakıt bütçesi, uyku |
| Hafta | 7 gün | Baskın döngüsü, stok tükenmesi, yakın kaynakların bitmesi, hastalık | Üs savunması, keşif seferi, taşınma | Baskın, kaynak yenilenmesi, hastalık |
| Mevsim | ay | Kış hazırlığı, ekim/hasat penceresi, nehir seviyesi | Ne kadar yakıt ve yiyecek stoklanacak | Takvim, tarım, nehir |
| Yıl | yıllar | Ormanın azalması, toprak yorgunluğu, yapı aşınması | Sürdürülebilirlik | Yenilenme, toprak, bakım |

### 6.3 Riski zorunlu kılan mekanizmalar

1. **Yerel tükenme:** Kampın etrafındaki ölü dal, yüzey taşı, kolay av ve yabani bitki tükenir; yenilenme yavaştır. Oyuncu her hafta biraz daha uzağa gitmek zorundadır.
2. **Belirsiz hava:** Tahmin vardır ama kesin değildir (bulutlar, rüzgâr yönü, basınç belirtileri). Yanlış tahmin yolda yakalanmak demektir.
3. **Gün ışığı:** Karanlıkta görüş, ısı ve güvenlik düşer; meşale yakıt tüketir.
4. **Yük ve sefer sayısı:** Az yükle çok sefer zaman ve kalori kaybettirir; ağır yük düşme ve donma riskini artırır.
5. **Isı bütçesi:** Ateş yakıt ister, yakıt dışarıdadır, dışarı soğuktur.
6. **Karşılıklı bağımlılık:** Ateşi gece yanık tutmak uykuyu böler; uyumamak yorgunluğu artırır; yorgun karakter dışarıda hata yapar.
7. **Kalıcı sonuç:** Rahatsızlıklar ve kayıplar kalıcı iz bırakır (donmuş parmak işlem hızını düşürür).

### 6.4 Görevler — beden ve durumlar (TLD)

- [ ] **RISK-B01 (P0)** — Kalori deposu + aktivite maliyet tablosu (yürüme, koşma, taşıma kütlesi, balta, kazma, sürme, titreme). *Mevcut:* `FWorldSurvivalProfile` hunger/thirst/stamina/fatigue. *Kabul:* 2 kütükle 1 km, boş yürümeden tablo oranında fazla kalori.
- [ ] **RISK-B02 (P0)** — Su ve içilebilirlik (göl, yağmur, kar; kaynatma).
- [ ] **RISK-B03 (P0)** — Genel sağlık bağlantısı: herhangi bir depo sıfırsa sağlık düşer.
- [ ] **RISK-B04 (P0)** — Uyku: saati ilerletir (shared clock tek kez), soğukta uyumak tehlikeli, uyandırma koşulları (soğuk, hayvan, ateşin sönmesi). *Kabul:* 8 saat uyku tüm sistemleri bir kez 8 saat ilerletir.
- [ ] **RISK-B05 (P0)** — Rahatsızlık sistemi (data-driven): hipotermi (birikimli risk → hastalık), donma (vücut bölgesi, kalıcı iz), gıda zehirlenmesi, burkulma (ağır yükle eğimde düşme), kanama, enfeksiyon riski. *Kabul:* her biri tetik → etki → tedavi zinciriyle test senaryosunda gösterilir.
- [ ] **RISK-B06 (P1)** — Kalıcı iz: donmuş parmak (işlem hızı), kötü iyileşmiş burkulma (taşıma kapasitesi), yara izi. *Kabul:* kalıcı iz kayıtta tutulur ve ölçülebilir etki verir.
- [ ] **RISK-B07 (P1)** — Tedavi: bandaj (bez/yosun), atel (çubuk + bez), bitki çayları, dezenfektan (reçine/alkol P2).

### 6.5 Görevler — sıcaklık, giysi, ateş

- [ ] **RISK-H01 (P0)** — Tek hissedilen sıcaklık fonksiyonu: hava (saat + mevsim + hava olayı + yükseklik) + rüzgâr soğutması (yönlü) − barınak koruması + ısı kaynakları + giysi. *Mevcut:* `UpdateProtection(ClothingInsulation, ShelterProtection)` iki serbest sayı. *Kabul:* giysi ve barınak değeri item/aktörden türetilir.
- [ ] **RISK-H02 (P0)** — Isı kaynağı bileşeni (kamp ateşi, taş ocak, şömine, fırın aynı bileşen): ısı, yarıçap, yakıt, kalan süre, söndürme koşulları.
- [ ] **RISK-H03 (P0)** — Ateş yakma: çıra (ince kuru çubuk) + yakıt + tutuşturucu (çakmaktaşı + çelik, sürtünme, büyüteç P2); başarı olasılığı (rüzgâr, ıslaklık, beceri); başarısızlıkta tüketim kuralı. *Kabul:* rüzgârlı açık alanda başarı oranı barınaktakinden düşük.
- [ ] **RISK-H04 (P0)** — Ateşin yaşamı: yakıt ekleme, köz, söndürme; rüzgâr/yağmur riski; taş ocak koruması.
- [ ] **RISK-H05 (P0)** — Giysi yuvaları (10) ve özellikleri: sıcaklık, rüzgâr geçirmezlik, ıslanma, donma, hareket cezası, kütle, dayanıklılık.
- [ ] **RISK-H06 (P1)** — Islanma, kuruma ve donma döngüsü (yağmur, kar, nehirden geçiş — Bölüm 3).
- [ ] **RISK-H07 (P1)** — Oda bütünlüğü ve iç sıcaklık: çatı, duvar sızdırmazlığı (sıva), açıklıklar (Bölüm 1).
- [ ] **RISK-H08 (P1)** — Giysi üretimi ve onarımı: post, deri, yün, keten bezi (Bölüm 4 ve 7).

### 6.6 Görevler — yakıt ve tükenme

- [ ] **RISK-F01 (P0)** — Yakıt tanımları: çıra, çubuk, takoz, kütük, odun kömürü, kuru gübre (P2); enerji, yanma süresi, nem cezası.
- [ ] **RISK-F02 (P0)** — Yerel tükenme: ölü dal, yüzey taşı, yabani bitki ve kolay av için bölge başına stok ve yavaş yenilenme. *Bağımlı:* SOTF-T05. *Kabul:* kampın 100 m çevresindeki ölü dal 1 haftada biter; 2 hafta boyunca yenilenme yetersiz kalır.
- [ ] **RISK-F03 (P1)** — Yakıt ve yiyecek bütçe görünümü: stok ÷ tüketim = kalan saat/gün; defterde.
- [ ] **RISK-F04 (P1)** — Yiyecek bozulması (kondisyon), soğuk depo, kurutma, tütsüleme, tuzlama (Bölüm 7). *Kabul:* tütsülenmiş et taze etten tabloya göre uzun dayanır.

### 6.7 Görevler — hava, ışık ve belirsizlik

- [ ] **RISK-W01 (P0)** — Hava sistemi: sıcaklık, rüzgâr (hız + yön), yağış (tür + miktar), bulut, sis; geçişler deterministik seed ile. *Mevcut:* `EWorldWeatherState` + severity. *Kabul:* aynı seed aynı hava dizisi.
- [ ] **RISK-W02 (P1)** — Tahmin ve belirtiler: bulut tipi, rüzgâr dönüşü, hayvan davranışı, "eklem ağrısı" gibi işaretler; tahmin olasılık taşır ve bazen yanılır. *Kabul:* belirtiler bir sonraki 12 saatlik hava için %X doğrulukla (veri) ipucu verir.
- [ ] **RISK-W03 (P1)** — Soğuk dalgası ve fırtına olayları: önceden belirti, süre, şiddet; olay takvimi mevsime bağlı.
- [ ] **RISK-W04 (P1)** — Gün ışığı ve karanlık: görüş mesafesi, sıcaklık düşüşü, meşale/fener yakıtı, AI davranış değişimi.

### 6.8 Görevler — atmosfer (riskin hissedilmesi)

- [ ] **RISK-A01 (P0)** — Karakter geri bildirimi: soğukta nefes buharı, titreme (el sallanması, nişan zorluğu), yorgunlukta görüş bulanıklığı, açlıkta yavaşlama. HUD'da sayı yerine öncelikle beden dili. *Kabul:* HUD kapalıyken oyuncu karakterin üşüdüğünü anlayabilir.
- [ ] **RISK-A02 (P0)** — Ses: rüzgâr şiddeti ve yönü, ateşin çıtırtısı (sönmek üzereyken değişir), uzaktan kurt sesi, buzun çatırdaması, yağmurun çatıya vurması.
- [ ] **RISK-A03 (P1)** — Görüş: kar fırtınasında görüşün düşmesi, sis, gece karanlığının gerçekten karanlık olması; ışık kaynağının önemi.
- [ ] **RISK-A04 (P1)** — Minimal HUD seçeneği: durumlar yalnız kritik eşikte görünür.
- [ ] **RISK-A05 (P1)** — Barınak hissi: içeri girince sesin ve rüzgârın kesilmesi, ateşin ışığı, sıcaklığın yükselmesinin geri bildirimi.

### 6.9 Görevler — topluluk ölçeği (Frostpunk, küçültülmüş)

- [ ] **RISK-C01 (P2)** — Küçük yerleşimci grubu (2–5 NPC): SURV profili, barınak/yemek/ısı ihtiyacı, iş ataması. *Bağımlı:* SOTF-A01.
- [ ] **RISK-C02 (P2)** — Moral: yemek çeşidi, sıcaklık, iş yükü, kayıplar; düşük moral iş hızını düşürür.
- [ ] **RISK-C03 (P2)** — Bedelli kararlar: tayın kısma, gece vardiyası, hasta bakımı vs iş gücü; kalıcı kayıt ve ölçülebilir etki.
- [ ] **RISK-C04 (P2)** — Keşif seferi: NPC uzak noktaya gönderilir, süre ve risk; dönüşte kaynak/olay/yaralanma.

---

## 7. Hydroneer + Satisfactory — üretim ağları: bir ham maddeden onlarca ürün

### 7.1 Mekanik ayrıştırması

- **Hydroneer:** Kazılan toprak suda ayıklanır, cevher ve taş çıkar; cevher eritilir, kalıplarla farklı ürünlere dönüşür; makineler fiziksel modüllerden kurulur, su ve güç ile çalışır; her şey fiziksel nesnedir.
- **Satisfactory:** Her ham madde birçok ara ürüne, ara ürünler daha karmaşık ürünlere dönüşür; tarifler dakika başına oranla tanımlanır; bant kapasitesi sınırlıdır (Mk1–Mk6: 60, 120, 270, 480, 780, 1200 öğe/dk); yan ürünler başka zincirlerin girdisidir; alternatif tarifler aynı ürüne farklı yollar sunar; güç ağı dengelenmelidir.

Ortak çekirdek: **Ham madde → ara ürün → son ürün ağacı; her düğümün birden fazla kullanımı; yan ürünlerin değerlendirilmesi; fiziksel istasyon ve taşıma kapasitesi.**

### 7.2 Üretim ağaçları (MyProject3 ölçeği)

Ok (→) bir istasyon/eylemi, köşeli parantez istasyonu gösterir.

**Ağaç**
- Kütük → [enine/boyuna kesim] → uzunluk ve kesit varyantları → duvar, kiriş, direk, köprü, sal
- Kütük → [yontma] → kalas → [dilimleme] → plank → zemin, kapı, raf, sandık, kızak, kova (çember ile)
- Kütük → [oyma] → yalak (su/yem), oluk (su kanalı), kano (P2)
- Kütük → [yarma] → takoz → yakıt
- Takoz/kütük → [kömür ocağı: toprakla örtülü yavaş yanma] → **odun kömürü** → yüksek ısı (demir eritme, cam)
- Yanma → **kül** → potas → sabun (yağ ile), gübre (pH ve potas), sır (çömlek)
- Kabuk → [ıslatma/döğme] → kabuk lifi → ip; kabuk → tanen → **deri tabaklama**; kabuk → çatı kaplaması
- Reçine (çam yarası) → meşale yakıtı, yapıştırıcı (ok ucu bağlama), su geçirmezlik (sal, kova), zift (reçine + odun kömürü)
- Talaş/yonga → çıra, kompost, altlık, tütsü yakıtı
- Yaprak → altlık, kompost, çatı kaplaması (geçici)

**Çubuk ve bitkisel lif**
- Çubuk → kazık, çit, örgü, sepet, ok şaftı, yay, meşale, alet sapı, raf, üçayak, tuzak tetiği, atel
- Keten/ısırgan/kabuk → [ıslatma, döğme, tarama] → lif → [eğirme] → iplik → [örme/dokuma] → ip, halat, bez → giysi, çanta, bandaj, yelken (P2), olta
- Keten tohumu → [pres] → yağ → lamba yakıtı, ahşap koruma, yemek
- Saman → altlık, çatı, kerpiç katkısı, sepet, yakıt

**Taş**
- Çakmaktaşı → [yonga] → bıçak, ok ucu, kazıyıcı, balta başı (taş alet kademesi), kıvılcım taşı
- Kireçtaşı → [kireç ocağı + yakıt] → sönmemiş kireç → [su] → sönmüş kireç → harç (kum ile), sıva, badana, tarla kireci, deri işleme (kıl dökme)
- Kumtaşı → bileme taşı, değirmen taşı (yumuşak)
- Granit → değirmen taşı, temel, köprü ayağı, ocak taşı
- Çakıl → yol, drenaj, beton katkısı (P2)
- Taş blok/levha → duvar, zemin, basamak, kemer, set

**Kil ve kum**
- Kil → [şekillendirme] → çömlek/kap → [kurutma] → [fırın] → pişmiş çömlek → su kabı, tencere (pişirme), depolama küpü (bozulmayı yavaşlatır), kalıp (döküm)
- Kil → tuğla → [fırın] → pişmiş tuğla → ocak, baca, fırın astarı, duvar; kil → kiremit → çatı
- Kil + saman + su → kerpiç/sıva (örgü duvar için)
- Kum → harç katkısı, döküm kalıbı; kum + kül + yüksek ısı → cam (P2) → şişe, pencere, mercek (büyüteç ile ateş)

**Metal (kazı ve ayıklama ile)**
- Toprak (kazı) → [suda ayıklama] → bakır/demir cevheri, çakıl, değerli taş (P2)
- Demir cevheri + odun kömürü → [taş/kil fırın + körük] → sünger demir → [dövme: örs + çekiç] → çubuk demir → alet başı (balta, kazma, kürek, keski, bıçak), çivi, menteşe, zincir, kova çemberi, tuzak yayı, ok ucu
- Bakır cevheri → [eritme] → bakır → [kalıp] → tencere, levha, tel; bakır + kalay (P2) → bronz

**Hayvan**
- Et → taze, pişmiş, kurutulmuş, tütsülenmiş, tuzlanmış
- Post → [kazıma] → [tabaklama: tanen veya kül/kireç] → deri → giysi, ayakkabı, çanta, kemer, körük, su tulumu, sapan
- Yün → [yıkama, tarama, eğirme] → yün iplik → kalın giysi, battaniye
- Kemik → [kırma/öğütme] → kemik unu (gübre), iğne, olta iğnesi, alet; kemik + su → tutkal (P2)
- Bağırsak → [kurutma] → kiriş (yay kirişi, dikiş ipi), sucuk (P2)
- Yağ → [eritme] → don yağı → mum, sabun (kül ile), deri bakımı, lamba yakıtı
- Tüy → ok tüyü, yastık
- Süt → peynir, tereyağı (çömlek ile; bozulma süresi uzar)
- Gübre → kompost, tarla gübresi, kuru gübre yakıtı (P2)

**Tahıl ve yiyecek**
- Buğday → [harman] → tane + saman + kepek → [değirmen] → un → [fırın] → ekmek
- Arpa → yem, malt (P2)
- Patates, fasulye, lahana → kiler, çorba (tencere), turşu (tuz + çömlek)

### 7.3 Görevler — tarif ve istasyon altyapısı

- [ ] **NET-R01 (P0)** — Tek tarif şeması: girdiler, çıktılar (ana + yan ürün), süre, istasyon türü, yakıt/güç, gerekli alet ve kalite. Elle crafting, istasyon, ocak, fırın, değirmen aynı şema. *Mevcut:* `FWorldProductionRecipe`, `InventoryCraftingComponent`. *Kabul:* yeni tarif kod değiştirmeden eklenir; elle ve istasyonda aynı şema okunur.
- [ ] **NET-R02 (P0)** — Yan ürün zorunluluğu: her tarifin yan ürünleri tanımlanır ve dünyaya/çıktı yuvasına düşer (talaş, kül, kabuk, kepek, saman). Yan ürün kaybolmaz. *Kabul:* yontma işlemi talaş üretir; talaş kompost tarifine girer.
- [ ] **NET-R03 (P0)** — Ağ doğrulayıcı (editör scripti): tüm tarifleri grafik olarak çıkarır; her ham maddenin en az N son ürüne ulaştığını, çıkmaz uç (hiçbir yere gitmeyen ara ürün) olmadığını raporlar. *Kabul:* çıkmaz uç varsa script uyarı verir; rapor dosyası repo'da.
- [ ] **NET-R04 (P0)** — İstasyon durum makinesi: boşta, çalışıyor, girdi bekliyor, çıktı dolu, yakıt/güç yok, arızalı; sebep istasyon panelinde. *Kabul:* çıktı yuvası doluyken girdi tüketilmez.
- [ ] **NET-R05 (P1)** — Alternatif tarif: aynı ürüne farklı yol (taş balta vs demir balta başı; kerpiç vs tuğla duvar); keşif ve not ile kolaylaşır (Bölüm 5).
- [ ] **NET-R06 (P1)** — Kalite yayılımı: girdi kalitesi ve alet kalitesi çıktı kalitesini etkiler (kuru düz çubuk → iyi ok şaftı).

### 7.4 Görevler — istasyon zinciri (ilk set)

- [ ] **NET-S01 (P0)** — Taş fırın/ocak (kil + taş): pişirme, kurutma, çömlek pişirme (düşük ısı).
- [ ] **NET-S02 (P0)** — Kömür ocağı: kütük/takoz yığını + toprak örtü (kazı malzemesi) + süre → odun kömürü. *Kabul:* örtüsüz yığın kömür değil kül verir.
- [ ] **NET-S03 (P0)** — Kireç ocağı: kireçtaşı + yakıt (yüksek) → kireç.
- [ ] **NET-S04 (P1)** — Eritme fırını + körük: odun kömürü ile yüksek ısı; cevher → metal; körük deri gerektirir (hayvan zinciri bağlantısı).
- [ ] **NET-S05 (P1)** — Örs ve dövme: metal → alet başı, çivi, menteşe.
- [ ] **NET-S06 (P1)** — Tabakhane: post kazıma tezgâhı + tanen/kireç küpü (süre) + kurutma çerçevesi → deri.
- [ ] **NET-S07 (P1)** — Lif işleme: ıslatma havuzu (yalak!), döğme tahtası, tarak, el iği → iplik → ip/bez.
- [ ] **NET-S08 (P1)** — Çömlekçi tezgâhı: kil → kap, tuğla, kiremit, kalıp.
- [ ] **NET-S09 (P1)** — Değirmen: el değirmeni → su değirmeni (granit taşı).
- [ ] **NET-S10 (P1)** — Tütsü/kurutma rafı, tuzlama küpü.
- [ ] **NET-S11 (P2)** — Pres (yağ), cam fırını, kalay/bronz.

### 7.5 Görevler — kazı ve ayıklama (Hydroneer çekirdeği)

- [ ] **NET-D01 (P0)** — Kazılan toprak bir malzeme item'ıdır (kova/kürek); kazı ile tek işlemde doğar. *Bağımlı:* 7D-V08.
- [ ] **NET-D02 (P1)** — Suda ayıklama: toprak + tava + su kaynağı (göl, dere, yalak) → bölge × tabaka olasılık tablosu → cevher/taş/boş; deterministik seed + OperationId.
- [ ] **NET-D03 (P1)** — Oluk düzeneği: kütük oluk (`GutterLog`) + su akışı ile toplu ayıklama. *Kabul:* su verilen oluğa dökülen toprak otomatik ayıklanır.
- [ ] **NET-D04 (P1)** — Cevher tipleri (dar set): demir, bakır, (P2) kalay ve değerli taş; tabaka ve bölgeye bağlı dağılım.

### 7.6 Görevler — fiziksel taşıma, su ve güç ağı

- [ ] **NET-L01 (P1)** — İstasyon portları: giriş/çıkış yuvaları, tür (katı, dökme, sıvı), kapasite.
- [ ] **NET-L02 (P1)** — Taşıma bağlantıları (projeye uygun): el arabası/kızak hattı, kütük oluk ile su üzerinde yüzdürme, zipline; her birinin dakika başına kapasitesi. *Kabul:* kapasiteden fazla girdi tamponda bekler, kaybolmaz.
- [ ] **NET-L03 (P1)** — Tampon ve geri basınç: depo dolunca zincir sırayla durur; boşalınca sırayla devam eder.
- [ ] **NET-W01 (P1)** — Su kanalı ağı: kaynak → oluk/hendek zinciri → tüketici (ayıklama, değirmen, sulama, tabakhane, ıslatma havuzu); akış ve yön; kanal kesilirse tüketici durur. *Bağımlı:* DS-W01, 7D-V14.
- [ ] **NET-P01 (P2)** — Mekanik güç ağı: su çarkı (akarsu), rüzgâr (açık tepe), hayvan (çekiş); tüketiciler değirmen, bıçkı, körük; üretim < talep ise ağ yavaşlar. *Kabul:* durgun suda su çarkı güç üretmez.
- [ ] **NET-P02 (P2)** — Su gücüyle bıçkı: kütük → plank otomatik (SOTF elle kesimin ileri kademesi).

### 7.7 Görevler — ilerleme

- [ ] **NET-G01 (P1)** — Teknoloji kademesi doğal akışla: taş alet → kil ve ateş → kireç ve tuğla → odun kömürü → demir. Kademe kilitle değil, önkoşul malzemeyle açılır (odun kömürü olmadan demir eritilemez). *Kabul:* demir alet yolu, kazı + ayıklama + kömür ocağı + fırın + körük (deri) zincirinin tamamını gerektirir.

---

## 8. Ortak sistem eşleştirmesi — bir çekirdek, birçok oyun

| Ortak sistem | Hangi oyunlardan beslenir | Tek otorite (önerilen) | İlk görevler |
|---|---|---|---|
| Fiziksel malzeme sözleşmesi | SOTF, Hydroneer, DS | `PhysicalMaterialPiece` + item tanımı | SOTF-M01..M05 |
| Taşıma ve yük | SOTF, DS, TLD, Hydroneer | Genel taşıma bileşeni (`ULogCarryComponent` genişletilir) | SOTF-C01, DS-C02 |
| Yapısal destek ve çökme | SOTF, 7D, DS köprü | `WorldStructuralGraphSubsystem` (çoklu destek, malzemeler arası) | SOTF-D01, SOTF-M02, 7D-S02 |
| Smooth voxel arazi | 7D, Hydroneer, FS sürme, DS set/hendek | `WorldTerrainStateSubsystem` → yoğunluk tabanlı arazi + render/collision/nav adaptörü | 7D-V01..V10 |
| Su (nehir, sel, kanal, içme) | DS, FS, Hydroneer, TLD | Tek su durumu (seviye, akıntı, havza) | DS-W01..W04, NET-W01 |
| Hava ve mevsim | DS, TLD, Frostpunk, FS | Tek hava sistemi (shared clock + seed) | RISK-W01, DS-W04, FS-G02 |
| Isı ve hissedilen sıcaklık | TLD, Frostpunk, SOTF | Tek sıcaklık fonksiyonu | RISK-H01..H02 |
| Yakıt | TLD, Frostpunk, Hydroneer, Satisfactory | Tek yakıt bileşeni | RISK-F01, NET-S01..S04 |
| Rahatsızlık ve tedavi | TLD, 7D, Frostpunk | Data-driven rahatsızlık bileşeni | RISK-B05 |
| Bozulma ve zaman | TLD, FS | Shared clock + item kondisyonu | RISK-F04, FS-P01 |
| Rota grafiği ve maliyet | DS, 7D baskın, FS hayvan, SOTF yardımcı | Tek grafik + `RouteCostProvider` | DS-P01..P02, 7D-R04 |
| Tarif ve istasyon | Satisfactory, Hydroneer, FS, SOTF | Tek tarif şeması + ağ doğrulayıcı | NET-R01..R04 |
| AI hedef ve navigasyon | 7D baskın, SOTF yardımcı, FS hayvan ve yırtıcı, TLD av | Ortak AI katmanı | 7D-R04, FS-A11 |
| Keşif, not ve defter | Subnautica | Keşif aktörü + defter | SN-N01..N02 |
| Dünya değişim günlüğü | DS, 7D, FS | Kalıcı olay kaydı | DS-P03 |

Kural: Oyuna özel görev, tablodaki ortak sistem görevi bitmeden başlatılmaz.

---

## 9. Önerilen sıra — dikey dilimler

**Dilim A — Orman, malzeme ve ilk barınak**
SOTF-M01..M05, SOTF-T01..T04, SOTF-L01..L06, SOTF-K01..K05, SOTF-S01..S06, SOTF-C01, SOTF-B01..B03, SOTF-D01..D02, RISK-B01..B03, RISK-H01..H04, RISK-A01..A02.
*Kabul:* ağaç kes → kütük, dal (çubuk) ve kabuk çıkar → kütüğü kes, çubukları sivrilt, taş topla → taş temel + kütük duvar + çubuk çatı iskeleti → taş ocakta ateş (çıra şart) → içeride hissedilen sıcaklık yükselir → kalori düşüşü ölçülür → save/load sonrası her şey aynı.

**Dilim B — Yaşayan nehir ve kalıcı köprü**
DS-W01..W04, DS-X01..X03, DS-P01..P02, RISK-W01, SOTF-C02, DS-C01..C02.
*Kabul:* ilkbahar yağmurları nehri geçilemez yapar → oyuncu basamak taşı dener, akıntı kaydırır → taş ayaklı kütük köprü kurulur → rota kalıcı kısalır → büyük selde alçak bir köprü yıkılır, taş ayaklı köprü dayanır → yaz kuraklığında sığ geçit açılır.

**Dilim C — Smooth voxel kazı ve ilk kil/kireç zinciri**
7D-V01..V10, NET-R01..R04, NET-D01, NET-S01..S03, SOTF-S07.
*Kabul:* kazı cebi pürüzsüz yüzeyle kazılır → toprak ve kil item olur → kil çömlek ve tuğla pişirilir → kireçtaşı kireç ocağında yakılır → harçlı taş duvar örülür → kazı ve item tek işlemde, tekrarda çoğalmaz → save/load sonrası arazi aynı.

**Dilim D — Tarla ve iki hayvan**
FS-F01..F05, FS-G01..G04, FS-A01..A04, FS-P01, FS-G07 (buğday, patates, keten).
*Kabul:* parsel sür, taşları topla → buğday ve keten ek → mevsim büyütür, kuraklık verimi düşürür → hasat ve saman → tavuk yumurtlar, keçi sağılır → tarla raporu verimin hangi çarpandan düştüğünü gösterir.

**Dilim E — Kış: risk zorunluluğu**
RISK-B04..B05, RISK-H05..H07, RISK-F01..F04, RISK-W02..W04, RISK-A03..A05, SOTF-L07.
*Kabul:* yakındaki ölü dal tükenir → oyuncu fırtına belirtilerine rağmen uzağa gider → ıslanır, hipotermi riski artar → takozla kış stoku yapılır → uyku ve ateş dengesi kurulur.

**Dilim F — Baskın gecesi**
7D-R01..R07, 7D-S01..S03, SOTF-K05, SOTF-L06, 7D-R09.
*Kabul:* uyarı belirtileri → AI en zayıf noktayı seçer → sivri kazık hattı ve hendek yolu pahalılaştırır → kazıcı duvarın altından tünel açar → duvar kırılır, üst sıra çöker → baskın sonrası onarım.

**Dilim G — Notlar ve serbest keşif**
SN-N01..N06, SN-E01..E02.

**Dilim H — Demir ve üretim ağı**
NET-D02..D04, NET-S04..S08, NET-L01..L03, NET-W01, NET-G01, FS-A09.
*Kabul:* ayıklanan demir cevheri + kömür + deri körük → demir balta başı; ağ doğrulayıcı her ham maddenin en az N ürüne ulaştığını raporlar.

---

## 10. Kapsam dışı

- 7DtD blok/küp inşa sistemi, şekil menüsü, blok yükseltme.
- Çok oyunculu ve asenkron paylaşım.
- Motorlu araçlar ve lisanslı makine filoları.
- Şehir ölçeğinde nüfus ve kanun ağaçları.
- Elektrik, nükleer, tren gibi sanayi sonrası kademeler.
- Su altı açık dünya.

Bu listede değişiklik bir tasarım kararıdır ve tarih ile kaydedilir.
