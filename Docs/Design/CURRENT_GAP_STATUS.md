# MyProject3 — Güncel Eksik Listesi (119 mekanik + derin görev analizi)

Tarih: 26 Ekim 2026
Kod taban: `integration/canonical-log-model` (9a6f6c5) — #2–#30 zinciri + #25 kütük hattı, tek dal.
Önceki belgeler: [MECHANICS_119_MASTER.md](MECHANICS_119_MASTER.md), [REFERENCE_GAMES_DEEP_TASKS.md](REFERENCE_GAMES_DEEP_TASKS.md).

## 1. Son analizden bu yana değişenler

| Konu | Önce | Şimdi |
|---|---|---|
| Derleme | #30 UHT'de kırık (`ELogCutAction`, sonra `HarvestableTreeActor.h:100`) | **`MyProject3Editor` UE 5.8'de derleniyor** (UHT + compile + link). 1 uyarı: `LogCarryComponent.cpp:59` C4701 |
| Kütük modeli | #24 ve #25 iki ayrı sözleşme | **#25 kanonik**: `ELogLengthFraction` + `ELogCrossSectionProfile`; #26'nın mesh aile setleri bu modele taşındı |
| Uzunluk kesme (M015) | PR'sız dalda, ana zincirde yok | Entegre: `SplitLengthHalf`, `SplitLengthThreeQuarter`, `SplitHalfLengthIntoQuarters`; atomik bölme + sahiplik + graph |
| PR zinciri | 29 taslak PR, yığılmış ve kopuk | Tek entegrasyon dalı; birleştirilirse #2–#30 ve #25'in yerini alır |
| Derleme hataları | Bilinmiyordu | 5 gizli derleme hatası bulundu ve düzeltildi |

Tamamlanan "Tüm Açık Eksikler" görevleri: **REL-02** (derleme yeşil), **REL-03** (kanonik kütük entegrasyonu), **REL-04** (zincir hizası tek dalla çözüldü). Kısmen: **SOTF-L01** (uzunluk + kesit kanonik; yüz sayısı, uç şekli, çentik, açıklık, nem, durum yok).

## 2. Hâlâ geçerli kesin sınırlar

- Hiçbir mekanik oyun içinde (PIE) çalıştırılarak test edilmedi → **S4 = 0**.
- Repoda **hiç otomasyon testi yok**.
- Repoda **hiç `.umap`/`.uasset` yok**; demo haritası yok.
- Kayıt dosyası (`MECHANIC_REGISTRY_119.csv`) 119/119 `not_run`. Entegrasyon dalı için `UnrealBuild = pass` artık kanıtla yazılabilir; `RuntimeTest` yazılamaz.

## 3. 119 mekanik — kalan eksikler

Etiketler: **Yok** (kod yok), **Enum** (yalnız tip), **S1** (tek sistemde kod), **S2** (sistemler arası bağlı kod). Tümü derleniyor, hiçbiri oyunda test edilmedi.

Özet: **Yok/Enum 33 · S1 63 · S2 23 · S4 0** (M015 S1 → S2 oldu).

### 3.1 Çekirdek (M001–M008)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M001 | Kalıcı kimlik | S2 | Envanterdeki item ↔ dünya aktörü kimlik kuralı; yinelenen kimlikle yükleme reddi testi |
| M002 | Tek yaşam döngüsü durumu | S1 | Tek durum makinesi (Dünya/El/Envanter/Depo/Taşıma/Rezerve/Tüketildi/Yok edildi); bayrak çiftleri çelişebiliyor |
| M003 | Atomik işlem | S2 | Çok aktörlü işlem (graph + yok etme + söküm) atomik değil; rollback başarısızsa her yerde "fault" yok |
| M004 | Yapılandırılmış sonuç | S1 | Birçok API hâlâ `bool`; ortak sonuç yapısı ve UI metni eşlemesi |
| M005 | Commit olayları | S1 | Tekilleştirme/yeniden oynatma; olay tüketicisi yok denecek kadar az |
| M006 | Saat ve seed | S2 | Takvim/mevsim katmanı; uyku ile zaman atlama sözleşmesi |
| M007 | Sürümlü kayıt | S2 | Çalışma zamanında doğan aktörleri yeniden yaratma yok; göç yolu tek yerde değil |
| M008 | Ortak survival durumu | S1 | Vücut ısısı ayrı değil; kalori yerine yüzde; giysi/barınak serbest sayı |

### 3.2 Kütük ve ahşap (M009–M028)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M009 | Ağaç devirme | S2 | Vuruş yönüne göre devrilme; devrilme hasarı; alet kalitesi |
| M010 | Ağaçtan kütük | S1 | Ağaç data asset'i; dal → çubuk; kabuk/reçine yan ürünü |
| M011 | Kütük taşıma | S2 | 2 kütük kuralı; kütleye göre hız/stamina; çubuk ve taşı da taşıyan ortak bileşen |
| M012 | Bırakma | S2 | Fırlatma ve çarpma hasarı |
| M013 | Yuvarlama/sürükleme/itme | S1 | Eğim ve kütle maliyeti; stamina; yerleşik kütükte red testi |
| M014 | Çarpma hasarı | Yok | Tamamı |
| M015 | Enine (uzunluk) kesim | **S2** | **Oyuncu C tuşu döngüsünde uzunluk kesme eylemleri yok** (oyundan erişilemiyor); vuruş noktasına göre kesim yok; 3/4 için 450 cm mesh yok; 75 cm mesh'lerin enum karşılığı yok |
| M016 | Boyuna yarma | S2 | İkinci parça ofsetinin ölçülmüş mesh'e dayanması; FBX manifesti |
| M017 | Çeyrekleme | S2 | `*_Quarter_04` anlamı; manifest |
| M018 | Yontma/kalas | S1 | `Hewn1`/`Hewn2` ayrımı (tek enum); Round → 1 yüz → 2 yüz → kalas zinciri; talaş |
| M019 | Oluk yalnız yarmadan sonra | S1 | Negatif durum testi; oluğun su/ayıklama işlevi |
| M020 | Yalak/kanal | S1 | Su/yem tutma bileşeni; su ağı |
| M021 | Plank | S1 | Çap bandına göre verim; hacim dengesi |
| M022 | Çentik ve geçme | Enum | Eylemler, mesh'ler, köşe kilidi |
| M023 | Sivri uç (kazık) | Yok | Tamamı |
| M024 | Snap anchor | S2 | Malzemeler arası anchor uyumluluğu |
| M025 | Yatay/dikey/çapraz yerleştirme | S1 | Mod başına zemin kuralı; önizleme = commit; red sebebi UI |
| M026 | Ebeveyn–çocuk bağlantı | S2 | Tek ebeveyn → çoklu destek |
| M027 | Destek kaldırma ve çökme | S1 | Fiziksel çökme; çöken parça sahipliği |
| M028 | Onarım, söküm, yakma | Yok | Tamamı |

### 3.3 Taş (M029–M038)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M029 | Toplama, tutma, fırlatma | S1 | Boyut bandı, kütle, tür; fırlatma; ortak taşıma bileşeni |
| M030 | Şekillendirme | Enum | İşleme fonksiyonu yok; eylem grafiği; mesh'ler |
| M031 | Tek taş yerleştirme | S1 | Oturma yüzeyi mantığı; önizleme |
| M032 | İstifleme | S1 | Derz kaydırma kuralı; bağlantı gücü |
| M033 | Duvar ve köşe | Enum | Birleşim kuralı; harç |
| M034 | Sütun, lento, açıklık | Enum | Davranış; açıklık limiti; kemer |
| M035 | Destek ve çökme | S1 | Malzeme yük profili; fiziksel çökme |
| M036 | Hasar, onarım, söküm | S1 | Onarım; söküm |
| M037 | Toplu depolama ve taşıma | S1 | Taş tutucu; kızak |
| M038 | Karma ahşap–taş | Yok | **Tür kilidi**: taş yalnız taşa, çubuk yalnız çubuğa bağlanıyor; ortak anchor bileşeni + uyumluluk tablosu |

### 3.4 Çubuk (M039–M048)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M039 | Toplama ve kırma | S1 | Kaynaklar; uzunluk/kalınlık/nem; demet taşıma |
| M040 | Sivriltme | S1 | Ateşte sertleştirme; alet şartı |
| M041 | İlkel alet | S1 | Çubuk durumu ile ürün item'ının ayrılması; tarif; dayanıklılık |
| M042 | Mızrak, yay, ok | S1 | Ayrı item'lar; tarif; mermi/hasar |
| M043 | Meşale | S1 | Yanma süresi; ışık; yakıt bağlantısı |
| M044 | Kazık, çit, bariyer | S1 | Zemin türü kuralı; ray; temas hasarı |
| M045 | Tuzak | S1 | Tetik algılama; hasar; tarifler |
| M046 | Raf, mobilya | S1 | İşlevsel aktörler |
| M047 | Bahçe ve hayvan yapıları | Yok | Tamamı |
| M048 | Söküm | S1 | Söküm işlemi ve verim |

### 3.5 Envanter ve üretim (M049–M058)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M049 | Dünyadan envantere | S2 | Taş/çubuğun aynı yolla alınıp yeniden doğması |
| M050 | İstif ve taşıma limiti | S2 | Bilinmeyen kütle kuralı; elde taşınanla tek kütle; aşırı yük kademeleri |
| M051 | Kategori ve hızlı erişim | S1 | UI; dayanıklılık/kondisyon alanları |
| M052 | Üretim alanına koyma | Yok | Üretim alanı aktörü; giriş yuvaları |
| M053 | Tarif ve alternatif tarif | S1 | Tek tarif şeması; alternatif tarif |
| M054 | Eksik malzeme gösterimi | S1 | Sebep kodu ve UI |
| M055 | Atomik üretim | S2 | Yan ürün çıkışı; hata enjeksiyonu testi |
| M056 | Alet koruma, artan iadesi | S1 | Alet dayanıklılığı; artan iadesi |
| M057 | İptal ve toplama | S1 | Elle üretimde iptal; kısmi ilerleme kuralı |
| M058 | Onarım kiti, koruma, tuzak parçası | Yok | Tarifler ve işlevler |

### 3.6 Ortak yapı (M059–M070)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M059 | Tek otoriter graph | S2 | Aktörlerdeki ebeveyn kopyası; malzemeler arası bağlantı |
| M060 | Zeminden yüke destek yolu | S1 | Gerçek zemin denetimi (bayrak yerine); arazi revizyonu |
| M061 | Açıklık/konsol/yük limiti | S1 | Açıklık, konsol, çoklu destekte yük paylaşımı |
| M062 | Hasar eşiği | S2 | Hasar görseli; hasar kaynağı kimliği |
| M063 | Kararlı sırada çökme | S1 | Yük yeniden dağıtımı; fiziksel çökme |
| M064 | Kapı/pencere/açıklık | Yok | Tamamı |
| M065 | Zemin, çatı, rampa, merdiven | Yok | Tamamı |
| M066 | Köprü, ip geçit | Yok | Tamamı |
| M067 | Avlu kapısı, çit, savunma duvarı | Yok | Tamamı |
| M068 | Hasar ve onarım | S1 | Onarım yolu; görsel kademe |
| M069 | Söküm ve geri kazanım | Yok | Tamamı |
| M070 | Destekleyen parçayı korumak | S2 | Çökme önizlemesi UI |

### 3.7 Arazi ve dünya (M071–M080)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M071 | Kalıcı chunk | S1 | Chunk + yoğunluk (SDF) modeli; **mimari karar (7D-V01) verilmedi** |
| M072 | İzinli katmanlar | S1 | Korunan bölgeler; tabaka tablosu |
| M073 | Geometri/collision ile çukur | Yok | Render/collision adaptörü |
| M074 | Tünel ve oda | Yok | Voxel çekirdeği |
| M075 | Yer altı çökmesi | Yok | Tamamı |
| M076 | Kazıların kalıcılığı | S1 | `Receipts` hâlâ `Transient` ve restore'da `Reset()`; chunk delta |
| M077 | Su seviyesi, akış, sel | Yok | Tamamı |
| M078 | Toprak nemi, besin, akış | S1 | Hücre katmanları; yüzey akışı |
| M079 | Yangın, çürüme, maruziyet | Yok | Tamamı (FIRE-01) |
| M080 | Yıkıntı ve ipucu | Yok | Keşif noktası aktörü; sahneler; içerik |

### 3.8 Tarım ve hayvancılık (M081–M090)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M081 | Deterministik sim adımı | S2 | Mevsim takvimi |
| M082 | Ekim ve koşul doğrulama | S1 | Parsel/hücre; tohum tüketimi; mevsim |
| M083 | Evrelerle büyüme | S1 | Ürün data asset'i (sabit 5 evre yerine) |
| M084 | Sulama, besin, stres | S1 | Besin, pH, ot; su item'ı ile sulama |
| M085 | Hasat ve verim | Yok | Tamamı |
| M086 | Hayvan besleme | S1 | `FeedAnimal` item tüketmiyor; yemlik/suluk aktörü; tür tanımı |
| M087 | Büyüme ve sağlık | S1 | Büyüme evresi; temizlik; hastalık |
| M088 | Hayvan ürünü | S1 | `ProductUnits` sayaç → item olarak doğma |
| M089 | Üreme | Yok | Tamamı |
| M090 | Yem–gübre–toprak döngüsü | Yok | Tamamı |

### 3.9 Baskın ve tehdit (M091–M100)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M091 | Zamanlama | S2 | Uyarı belirtileri; gün sayısına bağlı artış |
| M092 | Zorluk değerlendirme | S1 | `Pressure` alanının girdi formülü |
| M093 | Dalga ve bütçe planı | Yok | Tamamı |
| M094 | Doğuş kapasitesi | Yok | Tamamı (tek Pawn) |
| M095 | Doğuş konumu | S1 | Doğuş bölgesi çözümleyici |
| M096 | Hedef ve navigasyon | Yok | Tamamı |
| M097 | Saldırı ve yapı hasarı | S1 | Erişim tabanlı saldırı; kazıcı düşman |
| M098 | Dalga durum makinesi | S2 | Uyarı aşaması; dalga bazlı durumlar |
| M099 | İptal ve kurtarma | S1 | Kayıt sırasında aktif baskın kuralı; Pawn temizliği |
| M100 | Gürültü/stok/savunma baskısı | Yok | Tamamı |

### 3.10 Lojistik, üretim, survival, keşif (M101–M119)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M101 | Araba/kızak/raf yükleme | S1 | Kızak ve araba aktörleri |
| M102 | İp hattı/zipline | S1 | Fiziksel taşıyıcı ve hat aktörü |
| M103 | İlkel bıçkı ve zincir | S2 | Kütük dışı malzemeler; yan ürün; ağ doğrulayıcı |
| M104 | Kuyruk ve engel sebebi | S1 | Tam durum makinesi; UI |
| M105 | Yakıt, ısı, pişirme | Yok | Tamamı |
| M106 | Yol ve köprü ile rota maliyeti | S1 | `RouteCostProvider`; aktör uç noktaları; ortak grafik |
| M107 | Barınak ve günlük seçimler | S1 | Oda algılama; yerel tükenme; bütçe görünümü |
| M108 | Kaynak/dayanıklılık/depo göstergesi | Yok | UI |
| M109 | Entegre kayıt/yükleme | S2 | Manifest/hash karşılaştırması; aktör yeniden yaratma |
| M110 | Hava ve mevsim | S1 | Sürekli değerler; mevsim; tahmin |
| M111 | Soğuk/sıcak/ıslaklık | S1 | Tek sıcaklık fonksiyonu; rüzgâr soğutması |
| M112 | Açlık ve susuzluk | S1 | Kalori modeli; item etkileri |
| M113 | Stamina | S1 | Aktivite maliyet tablosu |
| M114 | Uyku ve yorgunluk | S1 | Uyku eylemi; zaman atlama |
| M115 | Yaralanma/hastalık/zehirlenme | Yok | Tamamı |
| M116 | Yiyecek bozulması | Yok | Tamamı |
| M117 | Giysi yalıtımı | S1 | Giysi yuvaları ve item özellikleri |
| M118 | Barınak kalitesi | S1 | Oda algılama ve bütünlük puanı |
| M119 | Keşif kalıcılığı | S1 | Keşif aktörü; defter; pratik bilgi |

## 4. Derin görev analizi (229 görev) — güncel durum

| Önek | Toplam | Tamam | Kısmi | Açık |
|---|---|---|---|---|
| SOTF (malzeme, kütük, çubuk, taş, inşa) | 62 | 0 | 3 | 59 |
| 7D (smooth voxel, baskın, yapısal bütünlük) | 32 | 0 | 0 | 32 |
| DS (yaşayan dünya, nehir, köprü, kargo) | 25 | 0 | 0 | 25 |
| FS (tarım, hayvancılık) | 41 | 0 | 0 | 41 |
| SN (notlar, keşif) | 9 | 0 | 0 | 9 |
| RISK (risk zorunluluğu, atmosfer) | 32 | 0 | 0 | 32 |
| NET (üretim ağları) | 28 | 0 | 0 | 28 |
| **Toplam** | **229** | **0** | **3** | **226** |

Kısmi olanlar:
- **SOTF-L01** — Kanonik geometri durumu: uzunluk + kesit tamam; yüz sayısı, uç şekli, çentikler, açıklıklar, nem, durum eksik.
- **SOTF-L04** — Uzunluk kesme: tarif tabanlı atomik kesim var; vuruş noktasına göre kesim ve oyuncu girişi (C döngüsü) yok.
- **SOTF-L05** — Kesim ilerlemesi: kayıtlı ilerleme kapısı var; görsel çentik/decal yok.

## 5. Öncelikli kalan işler (sırayla)

1. **Kayda kanıt yaz (REG-02):** Entegrasyon dalı derlemesinin `UnrealBuild` alanlarına işlenmesi. `RuntimeTest` değişmez.
2. **İlk otomasyon testleri (TEST-01..03):** sahiplik transferi, atomik bölme, uzunluk kesme, set karantinası. Bu, ilk S4 kanıtlarını mümkün kılar.
3. **Oyuncu erişimi:** uzunluk kesme eylemlerini `CycleCutAction` döngüsüne ekle (M015'i oynanabilir yapar).
4. **A dilimi çekirdeği:** SOTF-M01/M02 (ortak anchor bileşeni + malzemeler arası uyumluluk → M038), SOTF-D01 (çoklu destek → M026/M061), SOTF-C01 (ortak taşıma bileşeni → M011/M029/M039).
5. **Kütük tutarsızlıkları:** SOTF-L02 (75 cm / 3/4), SOTF-L03 (Hewn1/Hewn2/kalas zinciri), SOTF-M05 (FBX manifest scripti).
6. **Survival çekirdeği:** RISK-H01/H02 (tek sıcaklık fonksiyonu + ısı kaynağı → M105/M111), RISK-B01 (kalori → M112/M113).
7. **Mimari karar:** 7D-V01/V02 (smooth voxel: tüm dünya mı hibrit mi; eklenti mi kendi çekirdeğimiz mi). M071–M076 bu karar olmadan başlamaz.
8. **Demo haritası:** ilk `.umap` (DEMO-01); oyunda test için şart.
