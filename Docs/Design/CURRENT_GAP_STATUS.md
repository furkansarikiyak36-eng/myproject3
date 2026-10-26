# MyProject3 — Güncel Eksik Listesi (119 mekanik + derin görev analizi)

Tarih: 26 Ekim 2026
Kod taban: `integration/canonical-log-model` — #2–#30 zinciri + #25 kütük hattı + bu turdaki 8 adım.
Önceki belgeler: [MECHANICS_119_MASTER.md](MECHANICS_119_MASTER.md), [REFERENCE_GAMES_DEEP_TASKS.md](REFERENCE_GAMES_DEEP_TASKS.md).

## 1. Öncelikli 8 adımın durumu

| # | Adım | Durum | Kanıt |
|---|---|---|---|
| 1 | Kayda derleme kanıtı (REG-02) | Bitti | 90 mekanik `unreal_build = pass`, 119 `static_review = fail` (eksikler bu belgede) |
| 2 | İlk otomasyon testleri (TEST-01..03) | Bitti | 15 UE Automation testi, 15/15 geçiyor; 17 mekanik `automated_test = pass` |
| 3 | Uzunluk kesmeyi oyuncuya bağlama | Bitti | C döngüsünde dört uzunluk kesme ve ikinci yüz yontma |
| 4 | A dilimi çekirdeği (SOTF-M01/M02, D01, C01) | Bitti | Malzemeler arası bağlantı (M038), çoklu destek, ortak taşıma bileşeni; testli |
| 5 | Kütük tutarsızlıkları (SOTF-L02, L03, M05) | Bitti | 1/8 uzunluk, iki yüz yontma zinciri, FBX manifesti (`Docs/Architecture/LOG_ASSET_MANIFEST.md`) |
| 6 | Survival çekirdeği (RISK-H01/H02, B01) | Bitti | Tek hissedilen sıcaklık, ısı kaynağı bileşeni, kalori deposu; testli |
| 7 | Arazi mimari kararı (7D-V01/V02) | **Öneri, onay bekliyor** | Ölçülmüş prototip + `Docs/Architecture/TERRAIN_ARCHITECTURE_DECISION.md` (hibrit + kendi çekirdeğimiz) |
| 8 | Demo haritası (DEMO-01) | Bitti | `/Game/DemoAssets/Maps/VerticalSlice_A`, oyunda açıldı; görüntü: [VerticalSlice_A.png](VerticalSlice_A.png) |

Bu turda bulunan ve düzeltilen gizli hatalar:

- Bölmeden doğan ikinci kütük collision'sız kalıyordu, fizik almıyordu.
- Graph geri yüklemesi zemine oturan her kök parçayı reddediyordu; yerleştirilmiş yapı içeren her kayıt yüklemede başarısız olurdu.
- Voxel kazısında tam ızgara noktasındaki sıfır yoğunluk collision'da iğne deliği bırakıyordu.
- `main`'den gelen WASD bağlaması UE 5.8'de her açılışta `ensure` tetikliyordu.
- `LogCarryComponent.cpp` C4701 uyarısı giderildi; derleme artık uyarısız.

## 2. Hâlâ geçerli kesin sınırlar

- Mekanikler oyuncu eliyle (PIE'de etkileşimle) denenmedi → `runtime_test` 119/119 `not_run`, **S4 = 0**. Demo haritası oyunda açıldı ve görüntüsü alındı, ama tuşlarla oynanış denemesi yapılmadı.
- Otomasyon testleri 17 mekaniğin kabul ölçütünün yalnız bir kısmını kapsıyor; `static_review` bu yüzden hâlâ `fail`.
- Demoda taş, çubuk ve ateş için motorun temel şekilleri kullanılıyor; bu malzemeler için FBX yok.
- 75 cm ham yuvarlak kütük mesh'i yok; demo `SM_RawLog_Long_01`'i (ölçülen 75 cm) yerine kullanıyor. 3/4 uzunluk mesh'i yok.

## 3. 119 mekanik — kalan eksikler

Etiketler: **Yok** (kod yok), **Enum** (yalnız tip), **S1** (tek sistemde kod), **S2** (sistemler arası bağlı kod). Tümü derleniyor, hiçbiri oyunda test edilmedi.

Özet: **Yok/Enum 29 · S1 59 · S2 31 · S4 0** (A dilimi, kütük, survival, voxel ve demo işlerinden sonra).

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
| M011 | Kütük taşıma | S2 | Kütük için omuzda 2 yuva ve kütleye bağlı hız ortak yük bileşeninde; elde tutulan ikinci kütüğün görseli yok |
| M012 | Bırakma | S2 | Fırlatma ve çarpma hasarı |
| M013 | Yuvarlama/sürükleme/itme | S1 | Eğim ve kütle maliyeti; stamina; yerleşik kütükte red testi |
| M014 | Çarpma hasarı | Yok | Tamamı |
| M015 | Enine (uzunluk) kesim | S2 | Vuruş noktasına göre kesim yok; 3/4 için 450 cm mesh yok (işlem gerekçesiyle reddediliyor) |
| M016 | Boyuna yarma | S2 | İkinci parça ofsetinin ölçülmüş mesh'e dayanması; FBX manifesti |
| M017 | Çeyrekleme | S2 | `*_Quarter_04` anlamı; manifest |
| M018 | Yontma/kalas | S2 | Yuvarlak → 1 yüz → 2 yüz → kalas zinciri kodda ve testli; talaş yan ürünü yok |
| M019 | Oluk yalnız yarmadan sonra | S1 | Negatif durum testi; oluğun su/ayıklama işlevi |
| M020 | Yalak/kanal | S1 | Su/yem tutma bileşeni; su ağı |
| M021 | Plank | S1 | Çap bandına göre verim; hacim dengesi |
| M022 | Çentik ve geçme | Enum | Eylemler, mesh'ler, köşe kilidi |
| M023 | Sivri uç (kazık) | Yok | Tamamı |
| M024 | Snap anchor | S2 | Malzemeler arası anchor kuralları var; çentik/geçme anchor tipleri yok |
| M025 | Yatay/dikey/çapraz yerleştirme | S1 | Mod başına zemin kuralı; önizleme = commit; red sebebi UI |
| M026 | Ebeveyn–çocuk bağlantı | S2 | Graph çoklu desteği var; kütük aktörleri ikincil desteği henüz oyuncu yerleşiminde kullanmıyor |
| M027 | Destek kaldırma ve çökme | S1 | Fiziksel çökme; çöken parça sahipliği |
| M028 | Onarım, söküm, yakma | Yok | Tamamı |

### 3.3 Taş (M029–M038)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M029 | Toplama, tutma, fırlatma | S2 | Ortak yük bileşeniyle alma/bırakma ve kütle var; boyut bandı, tür ve fırlatma yok |
| M030 | Şekillendirme | Enum | İşleme fonksiyonu yok; eylem grafiği; mesh'ler |
| M031 | Tek taş yerleştirme | S1 | Oturma yüzeyi mantığı; önizleme |
| M032 | İstifleme | S1 | Derz kaydırma kuralı; bağlantı gücü |
| M033 | Duvar ve köşe | Enum | Birleşim kuralı; harç |
| M034 | Sütun, lento, açıklık | Enum | Davranış; açıklık limiti; kemer |
| M035 | Destek ve çökme | S1 | Malzeme yük profili; fiziksel çökme |
| M036 | Hasar, onarım, söküm | S1 | Onarım; söküm |
| M037 | Toplu depolama ve taşıma | S1 | Taş tutucu; kızak |
| M038 | Karma ahşap–taş | S2 | Taş ↔ kütük ↔ çubuk birleşimi kural tablosuyla çalışıyor ve kayıttan geri kuruluyor; taş↔taş ve çubuk↔çubuk yerel geri kurulum hâlâ eksik |

### 3.4 Çubuk (M039–M048)

| ID | Mekanik | Durum | Kalan eksik |
|---|---|---|---|
| M039 | Toplama ve kırma | S1 | Ortak yük bileşeniyle 10'luk demet sınırı var; kaynaklar, uzunluk/kalınlık/nem yok |
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
| M050 | İstif ve taşıma limiti | S2 | Kütle tahmini ve yük sınırı tek bileşende; bilinmeyen kütle reddediliyor; aşırı yük kademeleri yok |
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
| M061 | Açıklık/konsol/yük limiti | S2 | Yük destekler arasında paylaşılıyor; açıklık ve konsol limiti yok |
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
| M071 | Kalıcı chunk | S1 | Smooth voxel chunk prototipi var; mimari öneri onay bekliyor; asenkron üretim, dikiş ve kalıcılık yok |
| M072 | İzinli katmanlar | S1 | Korunan bölgeler; tabaka tablosu |
| M073 | Geometri/collision ile çukur | S2 | Kazı yüzeyi ve collision aynı anda güncelleniyor (testli); kazı → item ve kalıcılık yok |
| M074 | Tünel ve oda | S1 | Voxel çekirdeği tüneli destekliyor; yatay tünel kazma aracı ve navmesh yok |
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
| M105 | Yakıt, ısı, pişirme | S2 | Isı kaynağı bileşeni yakıtla, yağmurla ve ortak saatle çalışıyor; pişirme tarifleri yok |
| M106 | Yol ve köprü ile rota maliyeti | S1 | `RouteCostProvider`; aktör uç noktaları; ortak grafik |
| M107 | Barınak ve günlük seçimler | S1 | Oda algılama; yerel tükenme; bütçe görünümü |
| M108 | Kaynak/dayanıklılık/depo göstergesi | Yok | UI |
| M109 | Entegre kayıt/yükleme | S2 | Manifest/hash karşılaştırması; aktör yeniden yaratma |
| M110 | Hava ve mevsim | S1 | Sürekli değerler; mevsim; tahmin |
| M111 | Soğuk/sıcak/ıslaklık | S2 | Tek hissedilen sıcaklık fonksiyonu ve vücut ısısı var; ıslanma–kuruma döngüsü basit |
| M112 | Açlık ve susuzluk | S2 | Kalori deposu ve faaliyet maliyeti var; yiyecek item'ı tüketimi yok (ConsumeFood ham değer alıyor) |
| M113 | Stamina | S1 | Faaliyete göre stamina maliyeti var; dinlenme eylemi yok |
| M114 | Uyku ve yorgunluk | S1 | Uyku eylemi; zaman atlama |
| M115 | Yaralanma/hastalık/zehirlenme | Yok | Tamamı |
| M116 | Yiyecek bozulması | Yok | Tamamı |
| M117 | Giysi yalıtımı | S1 | Giysi yuvaları ve item özellikleri |
| M118 | Barınak kalitesi | S1 | Oda algılama ve bütünlük puanı |
| M119 | Keşif kalıcılığı | S1 | Keşif aktörü; defter; pratik bilgi |

## 4. Derin görev analizi (229 görev) — güncel durum

| Önek | Toplam | Tamam | Kısmi | Açık |
|---|---|---|---|---|
| SOTF (malzeme, kütük, çubuk, taş, inşa) | 62 | 7 | 4 | 51 |
| 7D (smooth voxel, baskın, yapısal bütünlük) | 32 | 0 | 8 | 24 |
| DS (yaşayan dünya, nehir, köprü, kargo) | 25 | 0 | 0 | 25 |
| FS (tarım, hayvancılık) | 41 | 0 | 0 | 41 |
| SN (notlar, keşif) | 9 | 0 | 0 | 9 |
| RISK (risk zorunluluğu, atmosfer) | 32 | 3 | 1 | 28 |
| NET (üretim ağları) | 28 | 0 | 0 | 28 |
| **Toplam** | **229** | **10** | **13** | **206** |

Tamamlananlar: SOTF-M01, SOTF-M02, SOTF-M05, SOTF-L02, SOTF-L03, SOTF-C01, SOTF-D01, RISK-H01, RISK-H02, RISK-B01.

Kısmi olanlar:
- **SOTF-L01** — Uzunluk + kesit + iki yüz yontma kanonik; uç şekli, çentikler, açıklıklar, nem, durum eksik.
- **SOTF-L04** — Tarif tabanlı uzunluk kesme ve oyuncu girişi var; vuruş noktasına göre kesim yok.
- **SOTF-L05** — Kayıtlı kesim ilerlemesi var; görsel çentik/decal yok.
- **SOTF-K02** — 10'luk çubuk demeti yük bileşeninde; demet düşünce saçılma yok.
- **7D-V01/V02** — Ölçülmüş öneri yazıldı; onay bekliyor.
- **7D-V03/V04/V05/V07/V08** — Tek chunk prototipi: yoğunluk, yüzey, collision, küresel kazı/doldurma, hacim ölçümü; asenkron üretim, malzeme kimliği, item üretimi yok.
- **7D-S02** — Yük çoklu destek arasında paylaşılıyor; malzeme yük profilleri yok.
- **RISK-H04** — Ateşin yakıtı ve yağmurda sönmesi var; köz ve yeniden yakma yok.

## 5. Sıradaki işler (önerilen sıra)

1. **Arazi kararını onayla (7D-V01/V02).** Onaydan sonra: asenkron mesh üretimi, chunk dikişleri, kazı → item (7D-V08), chunk delta kalıcılığı (7D-V10), navmesh (7D-V06).
2. **Demoyu oyuncu eliyle dene ve `runtime_test` kanıtlarını yaz.** Her mekanik için PIE'de pozitif ve negatif durum.
3. **A diliminin kalanı:** kütük kalıntısı ve dallar → çubuk (SOTF-T03/T04), devrilme yönü ve hasarı (SOTF-T02, M014), sivri uç (SOTF-L06, M023), çentik tipleri (SOTF-L08, M022).
4. **Çubuk ve taş derinliği:** çubukta geometri ile ürünü ayırma (SOTF-M04), taş boyut bandı ve tür (SOTF-S01), kuru duvar derz kuralı (SOTF-S04).
5. **Giysi ve barınak item'dan türesin:** RISK-H05 (giysi yuvaları), RISK-H07 (oda bütünlüğü); M117/M118 hâlâ serbest sayı.
6. **Yaşayan nehir ve köprü (B dilimi):** DS-W01..W04, DS-X01..X03, DS-P01..P02.
