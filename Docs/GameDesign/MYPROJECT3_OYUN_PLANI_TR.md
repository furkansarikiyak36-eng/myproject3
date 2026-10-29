# MyProject3 — Oyun Planı (baştan sona)

Tarih: 2026-10-29 · Durum: taslak, kullanıcı onayı bekliyor
Bu belge oyunun tasarımını ve geliştirme sırasını tek yerde toplar. **Adım adım görevler, dürüst durum ve kaldığı yerden devam etme kuralları: [`memory/ROADMAP.md`](../../memory/ROADMAP.md)** (herhangi bir asistan oradan devam eder). Teknik ayrıntı modül belgelerinde, kararların özeti `memory/DECISIONS.md` içinde.

---

## 1. Oyun tek cümlede

Oyuncu ilkel bir vadide, yalnız **ağaç, taş, kil, lif, kemik, ot, reçine** gibi ilkel malzemelerle hayatta kalır; mevsimlere hazırlanır, keşifle yeni kaynaklar bulur, dünyayı yol, köprü ve tarlalarla kalıcı olarak dönüştürür ve kendi yerleşimini kurar.

## 2. Temel ilkeler

1. **Önce dünya, en son hikâye.** Bütün mekanikler aynı dünya durumunu paylaşır. Hikâye hiçbir mekanik için yazılmaz; en son, var olan dünyanın üstüne konur.
2. **Her mekanik bir başkasını besler.** Tek başına duran, hiçbir şeye yaramayan mekanik oyuna girmez.
3. **Zaman ana kaynaktır.** Gün, ışık ve enerji sınırlıdır. Her iş zaman ister. Oyuncunun asıl kararı: "Bu haftaki zamanımı neye ayırıyorum?"
4. **Dünya önceden haber verir.** Olaylar rastgele düşmez; işaret verir, mevsime ve dünyanın durumuna bağlıdır, birbirini tetikler.
5. **Keşif büyümenin anahtarıdır.** Tohum, malzeme yatağı, bilgi ve iyi yerler dışarıdadır. Altyapı, keşfin maliyetini kalıcı olarak düşürür.
6. **Fiziksel montaj en son.** Sons of the Forest tarzı parça parça montajın geliştirme maliyeti yüksek. Dönüşüm listesi şimdi kilitlenir, fiziksel montaj son aşamada geliştirilir.

## 3. Ortak dünya durumu

Her sistem yalnız bunları okur ve yazar:

| Durum | İçerik |
|---|---|
| Eşya | Her eşya tek yerdedir: dünya, el, depo, yolda. Çoğaltma yok. |
| Zaman | Gün, gece, ay, mevsim, yıl. |
| Hava | Sıcaklık, yağış, rüzgâr, kar kalınlığı, olaylar. |
| Arazi | Katmanlar, kazılar, nem, su seviyesi, yollar. |
| Yapı | Destek grafiği, hasar, kapalılık (ısı kaybı). |
| Canlılar | Oyuncu ihtiyaçları, hayvanlar, ekinler. |
| Kayıt | Hepsi kaydedilir ve yüklenir. |

---

## 4. Kapsam: İlkel çağ

### 4.1 Malzeme grupları

| Grup | İlk kullanım | Nereden gelir |
|---|---|---|
| Taş, çakmaktaşı | Kesme, yontma, kazıma | Yüzey, kazı, çakmaktaşı yatağı (keşif) |
| Dal, kütük, ağaç kabuğu | Yapı, sap, yakıt | Ağaç kesme, toplama |
| Kemik, boynuz | Delme, av araçları, küçük parçalar | Av, hayvancılık |
| Lif, ip, deri | Bağlama, giysi, taşıma | Kabuk ve bitki lifi, av |
| Ot, saz, kamış | Sepet, ağ, örtü | Su kenarı, bataklık (keşif) |
| Kil, toprak, kum | Kap, sıva, zemin, yapı | Kazı (katmana göre), dere kıyısı |
| Reçine, bitki zamkı | Birleştirme, sızdırmazlık | Ağaca çentik (zamanla birikir) |
| Kömür, aşı boyası | Ateş sonrası üretim, boya | Kömür çukuru; aşı kayalığı (keşif) |
| Kabuk, diş, tüy | Süsleme, özel araçlar | Av, göl kıyısı (keşif) |

### 4.2 Kapsam kuralları

1. Her gruptan **en fazla 2–3 eşya** çıkar.
2. Her eşya **en az iki mekaniğe** yarar.
3. Liste dışı malzeme (metal, cam, tuğla…) ilkel çağa girmez; sonraki çağların işidir.
4. Yeni mekanik önerisi önce şu soruyu geçmeli: "Hangi mevcut mekaniği besliyor?"

### 4.3 Dönüşüm listesi (kilitli)

**E** = eşya dönüşümü (tarif kaydı, ucuz). **F** = fiziksel parça (mesh, kesim geometrisi, snap; pahalı; son aşama).

| # | Dönüşüm | Araç / istasyon | Tür |
|---|---|---|---|
| 1 | Çakmaktaşı → kesici yonga | Çekiç taşı | E |
| 2 | Yonga + sap + ip/reçine → balta, keski, kazıyıcı | El | E |
| 3 | Taş → yontulmuş taş blok | Keski | F |
| 4 | Taş → kırma taş | Çekiç taşı | E |
| 5 | Ağaç → kütük | Balta | F (var) |
| 6 | Kütük → kısa kütük | Balta | F (var) |
| 7 | Kütük → yarım → çeyrek | Balta, kama | F (var) |
| 8 | Kütük → yontulmuş / oluklu | Keski | F (var) |
| 9 | Yarım kütük → tahta | Kama, keski | F |
| 10 | Dal → çubuk / kazık | Kesici | F (kısmen var) |
| 11 | Ağaç → kabuk → lif | Kazıyıcı | E |
| 12 | Odun → yakacak | Balta | E |
| 13 | Lif → ip | El | E |
| 14 | Ham deri → kazınmış → işlenmiş deri | Germe çerçevesi | E |
| 15 | Deri + iğne + ip → giysi, çanta | El | E |
| 16 | Kemik/boynuz → iğne, uç, olta | Bileği taşı | E |
| 17 | Saz → çatı demeti | El | F |
| 18 | Ot/saz → sepet, ağ, hasır | El | E |
| 19 | Kazı → kil / toprak / kum | Kazıyıcı, kazma çubuğu | E (arazi) |
| 20 | Kil + kum + su → sıva; duvara sürme | El | E (duvar durumu) |
| 21 | Kil → çiğ kap → pişmiş kap | Çukur fırın + kömür | E |
| 22 | Toprak → sıkıştırılmış zemin, set | El | E (arazi) |
| 23 | Ağaca çentik → reçine | Kesici + zaman | E |
| 24 | Reçine + kömür tozu → zift | Ateş | E |
| 25 | Odun → kömür | Kömür çukuru | E |
| 26 | Aşı toprağı → boya | Öğütme taşı | E |
| 27 | Tüy + çubuk + uç + reçine → ok | El | E |
| 28 | Diş/kabuk → kazıyıcı, süs | Bileği taşı | E |
| 29 | Çiğ et/balık → pişmiş → tütsülenmiş | Ateş, tütsü rafı | E |
| 30 | Su → kaynamış su; kum + kömür → süzgeç | Kil kap + ateş | E |
| 31 | Dal → kazma çubuğu (ateşte sertleştirme) | Ateş | E |
| 32 | Boynuz + sap + ip → çapa | El | E |
| 33 | Ahşap + yonga + reçine → orak | El | E |
| 34 | Ürün → kurutulmuş ürün | Kurutma rafı | E |
| 35 | Ürün → tane + sap | Harman yeri, döven sopası | E |
| 36 | Tane → temiz tane | Savurma sepeti | E |
| 37 | Tane → un | Öğütme taşı | E |
| 38 | Kül + ot + artık → kompost | Kompost yığını | E |

**Toplam: 38 dönüşüm, 8 fiziksel parça, 8 istasyon.** İstasyonlar: çalışma kütüğü, germe çerçevesi, çukur fırın, kömür çukuru, tütsü rafı, bileği/öğütme taşı, kurutma rafı, harman yeri.

İstasyon kuralı: girdi gerçekten tüketilir, yakıt gerçekten yanar, çıktı için yer gerekir, girdi biterse iş durur.

---

## 5. Sistemler ve bağlantıları

### 5.1 Dışarısı: ihtiyaç, çevre, yol

| Sistem | İçerik | Besler / beslenir |
|---|---|---|
| İhtiyaçlar | Açlık, susuzluk, yorgunluk ve uyku, sıcaklık, ıslaklık, hastalık, yara | Yiyecek, su, ateş, barınak, giysi |
| Bozulma | Yiyecek zamanla bozulur; nem ve sıcaklık hızlandırır | Kiler, tütsü, kurutma, kil küp |
| Ekipman | Giysi (deri), çanta, sepet; taşıma kapasitesi | Av, deri işleme, keşif |
| Isı | Yakıt yanar → ısı; barınağın kapalılığı ısıyı tutar; sıva boşlukları kapatır | Odun, kömür, yapı, sıva |
| Hareket | Yük ağırlığı, eğim ve zemin (çamur, kar, buz) hızı ve enerjiyi etkiler | Yol, köprü, kızak, ip hattı |

### 5.2 Üs: amaç ve strateji

Üssün amacı yalnız savunmak değil, **dışarının baskısına karşı korunacak şeyleri barındırmaktır**:

| Korunacak | Neye karşı |
|---|---|
| Erzak | Bozulma, nem, kemirgen, hayvan, yağmacı |
| Yakıt yığını | Yağmur, uzun kış |
| Ateş ve ısı | Rüzgâr, kar, yakıtın bitmesi |
| Su | Kuraklık, donma, kirlenme (kül yağmuru) |
| Tarla | Don, sel, dolu, hayvan |
| Üretim istasyonları | Girdisiz kalma, yangın, hasar |
| Yapılar | Kar yükü, fırtına, çığ, baskın |

Her yer kararının bir bedeli vardır:
- **Yer altı kiler:** serin tutar ama alçakta sel basar.
- **Yüksek yer:** selden korur ama soğuktur ve görünür.
- **Ateşe yakın depo:** pratiktir ama yangın riski taşır.
- **Dere kenarı:** su ve taşıma sağlar ama sel ve nem getirir.

Yer altının kullanımları: kiler, fırtına ve kış sığınağı, gizli depo, kil ve taş ocağı, kuyu ve sarnıç, hendek ve tünel. Desteksiz kazı çöker. Kazının derinlik ve hacim sınırı vardır.

### 5.3 Savunma

- Baskın bir hedef değil, tehditlerden biridir. Dünyadaki işaretlerle gelir: depo büyüklüğü, yiyecek kokusu, gece ışığı, duman.
- Baskıncılar hedef seçer (depo, ateş, hayvan). Hasar yapı grafiğine gerçekten yansır.
- Savunma bir **zaman işidir**: çite harcanan gün, erzağa harcanmamış gündür.
- Araçlar: kazık çit, hendek, set, gizli yer altı deposu, ışık ve koku yönetimi, onarım.

### 5.4 Üretimin üç basamağı

1. **Elle:** Oyuncu keser, yontar, taşır. Yavaştır ama esnektir. Oyunun başı böyle geçer.
2. **İstasyonla:** İstasyon oyuncu yokken çalışır. Oyuncu zamanını geri kazanır.
3. **Plan ve yardımcıyla:** Oyuncu planı çizer, yardımcı NPC çıktıyı taşıyıp kurar (`BuildPlanActor`, `HelperCompanionActor`).

---

## 6. Tarım ve hayvancılık (el aletleriyle)

### 6.1 Tarla zinciri

| # | Aşama | El aleti | Tarla durumu |
|---|---|---|---|
| 1 | Yer seçimi | — | Toprak türü, güneş, eğim, su uzaklığı |
| 2 | Temizleme | Balta, kazıyıcı | Çalı, kök, taş temizlendi (taş çite, çalı yakıta) |
| 3 | Toprak işleme | Kazma çubuğu, boynuz çapa | Gevşek |
| 4 | Gübreleme | Sepet | Verimlilik (kül, kompost, hayvan gübresi) |
| 5 | Ekim | Tohum kesesi | Sıra, aralık, ekim zamanı |
| 6 | Sulama | Kil kap, deri tulum, ark | Nem |
| 7 | Bakım | Çapa, saz malç | Ot baskısı, nem, don koruması |
| 8 | Koruma | Kazık + ip çit, korkuluk | Hayvan zararı |
| 9 | Hasat | Orak, kazıyıcı | Ürün + sap (geç kalınırsa kayıp) |
| 10–13 | Kurutma, harman, savurma, öğütme | Raf, döven, sepet, öğütme taşı | Tane, un, saman |
| 14 | Depolama | Kil küp, sepet, kiler | Bozulma, kemirgen |
| 15 | Tohum ayırma | — | Ye mi, sakla mı kararı |

- **Toprak yorulması:** Aynı ürün üst üste ekilirse verim düşer. Çözüm gübre, nadas ya da ürün değiştirmek.
- **Ekin türleri:** 3 tür önerisi: bir tahıl, bir kök bitki, bir baklagil. Her biri farklı bir bölgede ve mevsimde keşifle bulunur.

### 6.2 Hayvan zinciri

Yakalama (tuzak, ağ) → ağıl (kazık, ip, saz) → besleme ve su (saman, tahıl artığı) → ürün (yumurta, süt, tüy, gübre) → kesim (et, deri, kemik; malzeme listesine geri döner).

---

## 7. Takvim ve olaylar

Yıl 12 ay. Öneri: ay başına 5 oyun günü (yıl 60 gün), ayarlanabilir. İlk yıl yumuşak, sonraki yıllar daha sert.

| Ay | Hava | Olası olaylar | Tarla | Diğer |
|---|---|---|---|---|
| Ocak | Yoğun kar | Kar fırtınası, çığ, çatıda kar yükü | — | Isı; donan nehirden geçiş |
| Şubat | Ayaz | Çığ, kurtlar, kıtlık | Tohum kontrolü | Alet, deri işleri |
| Mart | Erime | Sel, çamur | Temizleme | Onarım; buz yolu kapanır |
| Nisan | Yağmur | Geç don, sel | İşleme, gübre, ekim | Kazı mevsimi |
| Mayıs | Ilık | Dolu | Sulama, ot alma | Balık göçü |
| Haziran | Sıcak | Dolu, fırtına | Bakım, malç | İnşa ve keşif |
| Temmuz | Kurak | Kuraklık | Sulama zorunlu | Uzun sefer (geçitler açık) |
| Ağustos | Kurak, yıldırım | Orman yangını → kül/zehirli yağmur | Erken hasat | Yemiş toplama |
| Eylül | Serin | Salgın, sis | Ana hasat, kurutma | Av göçü, reçine |
| Ekim | Yağmur, rüzgâr | Fırtına, sis | Harman, öğütme, depolama | Odun yığını, kiler |
| Kasım | İlk kar | Erken kar, baskın | Tohum ayırma, nadas | Son seferler, sıva |
| Aralık | Kar | Kar fırtınası | — | Kış sınavı |

### 7.1 Olaylar

| Olay | İşaret | Etki | Hazırlık |
|---|---|---|---|
| Kar fırtınası | Basınç düşer, hayvanlar saklanır | Dışarısı ölümcül | Yakıt, erzak, yalıtım |
| Çığ | Yoğun kar + ılık gün, çatırtı | Eğim altındaki yapı ve yol gömülür | Yer seçimi, kazık sırası |
| Kar yükü | Kar kalınlığı | Zayıf çatı çöker | Dik çatı, sağlam kiriş, temizleme |
| Sel | Erime + yağmur, dere yükselir | Alçak tarla, kiler ve depo su basar | Yüksek yer, set, hendek |
| Geç don | Açık, durgun gece | Filizler ölür | Malç, geç ekim |
| Dolu | Kara bulut | Ekin zararı | Örtü, ürün çeşitliliği |
| Kuraklık | Dere çekilir | Ekin kurur, su azalır | Sarnıç, ark, kil küp |
| Orman yangını | Kuraklık + yıldırım, duman | Ağaç ve yapı kaybı | Yangın şeridi, taş, sıva |
| Kül / zehirli yağmur | Yangın sonrası rüzgâr, gri gök | Açık su ve ekin kirlenir, dışarıdaki hastalanır | Kapalı kap, çatı, kum + kömür süzgeç, kaynatma |
| Salgın | Kirli su, bozuk yiyecek, ıslaklık | Oyuncu ve hayvan zayıflar | Temiz su, tütsü, kuru barınak |
| Fırtına | Rüzgâr döner | Çatı, çit, örtü hasarı | Bağlama, ağırlık, onarım |
| Sis | Sabah serinliği | Kaybolma, yırtıcı | İşaret taşları, yol, meşale |
| Baskın | Depo, koku, ışık, duman | Depo ve yapı hasarı | Çit, hendek, gizli depo |

### 7.2 Olay zincirleri

- Kuraklık → yangın → kül yağmuru → kirli su → salgın
- Yoğun kar → ılık gün → çığ
- Erime + yağmur → sel → kiler ve tarla kaybı
- Kötü hasat → kış açlığı → kurtlar ve baskınlar cesurlaşır

---

## 8. Keşif

### 8.1 Ödüller

| Ödül | Örnek |
|---|---|
| Malzeme yatağı | Çakmaktaşı, kil, saz, reçine ormanı, aşı kayalığı |
| Tohum ve canlı | 3 ekin türü, yakalanacak hayvanlar (oyuncu tohumsuz başlar) |
| Bilgi | Kalıntılar teknik öğretir (kil pişirme, set, süzgeç) |
| Yer | Mağara, sıcak su kaynağı, iyi üs ve ileri kamp yeri |
| Mevsimlik fırsat | Balık ve av göçü, kuruyemiş, donan nehir |
| Hikâye yeri | Şimdiden ayrılır, içerik en son gelir |

### 8.2 Bölgeler

| Bölge | Kaynak | Engel | Kalıcı çözüm |
|---|---|---|---|
| 0. Üs vadisi | Ağaç, taş, su | — | — |
| 1. Dere boyu | Kil, kum, saz, balık | Dere | Kütük köprü, basamak taşları |
| 2. Yüksek orman | Reçine, boynuz, av, çakmaktaşı | Yamaç, kayalık | Patika, merdiven, ip |
| 3. Göl ve bataklık | Kabuk, aşı boyası, nadir tohum | Bataklık, göl | Tahta yol, sal |
| 4. Dağ geçidi | Mevsimlik kaynak, hikâye yeri | Kar, soğuk, mesafe | Yazın açık; giysi, erzak, ileri kamp |

### 8.3 Altyapı

İşaret taşı ve patika, yol (kırma taş), köprü, ip hattı ve zipline, merdiven, sal, ileri kamp (ateş + depo).

Altyapı da dünyanın kurallarına tabidir: sel kötü köprüyü götürür, kar patikayı örter, fırtına ip hattını koparır, kamp deposuna hayvan dadanır. Bakım ister, yani zaman ister.

Döngü: keşfet → bul → geri getir → yeni dönüşüm açılır → tekrar gitmek gerekir → altyapı kur → kaynak üretime bağlanır → daha uzağa gitmeye zaman kalır.

---

## 9. Oyuncunun yolculuğu (örnek ilk yıl)

Oyun ilkbaharın başında başlar: Mart sonu, eriyen kar, çamur. Oyuncunun elinde hiçbir şey yoktur.

| Dönem | Oyuncu ne yaşar |
|---|---|
| İlk günler | Taş ve dal toplar, ilk kesiciyi yapar, ateş yakar, sundurma kurar. Islanmamayı, ısınmayı ve suyu kaynatmayı öğrenir. |
| İlkbahar | İlk kütükleri işler, küçük bir barınak kurar. Dere boyunu keşfeder: kil, saz, ilk yabani tohum. Sel, alçak yerin bedelini gösterir. |
| Yaz | Dere köprüsü kurulur, kil kap pişer, ilk tarla açılır. Yüksek ormanda çakmaktaşı ve reçine bulunur. Uzun sefer zamanıdır. |
| Yaz sonu | Kuraklık ve yangın riski başlar. Su deposu, yangın şeridi, kapalı kaplar gerekir. |
| Sonbahar | Hasat, kurutma, harman. Odun yığını ve kiler hazırlanır. İlk baskın, büyüyen deponun kokusuyla gelir. |
| Kış | Hazırlık sınanır: yakıt, erzak, yalıtım, çatıda kar yükü, çığ. Kapalı işler yapılır: deri, alet. |
| 2. yıl ve sonrası | Daha uzak bölgeler, ikinci ve üçüncü ekin, hayvan ağılı, istasyonlar ve yardımcı NPC. Vadi yollar, köprüler ve tarlalarla dolar. |

Zorunlu bir bitiş yok (hikâye en son eklenecek). Oyuncunun hedefleri dünyanın kendisinden doğar: kışı atlatmak, yeni bölgeye geçmek, yeni ekin bulmak, üssü güvene almak.

---

## 10. Repo durumu (2026-10-29)

| Alan | Durum |
|---|---|
| Eşya sahipliği, işlem defteri, kayıt | Kodda var; oyunda doğrulanmadı |
| Saat | `WorldSimulationClockSubsystem` var. Ay, mevsim ve takvim yok |
| Hava | 6 durum var (açık, yağmur, rüzgâr, kar, fırtına, sıcak hava). Olay zinciri, sel, çığ, yangın, zehirli yağmur yok |
| İhtiyaçlar | Kalori, susuzluk ve yorgunluk değerleri, hissedilen sıcaklık, aktivite var. Su eşyası, uyku, ıslaklık, hastalık ve bozulma yok |
| Isı, barınak | `HeatSourceComponent`, `ShelterSensorComponent` var |
| Taşıma | `CarryLoadComponent`, kargo ve lojistik altsistemleri var. Eğim ve zemin etkisi doğrulanmadı |
| Kazı | Voxel prototipi ve arazi durumu var. Katman → malzeme kuralı yok |
| Üretim | İstasyon, tarif ve iş kuyruğu var (`WorldProductionSubsystem`). Tarifler veride değil, demo aktöründe koda gömülü. 38 dönüşümün çoğu tanımlı değil |
| Tarım | Ekin aşamaları + nem + süre var ("koy, bitsin"). Tarla zinciri, alet şartı, tohum, yorulma yok |
| Hayvan | Açlık, sağlık, ürün var. Yakalama, ağıl, kesim zinciri yok |
| Tehdit | Tehdit yöneticisi ve baskın adaptörü var. İşaretle tetiklenme yok |
| Yapı | Yapı grafiği, hasar, kar yükü olmadan var |
| Fiziksel montaj | Kütük ve snap çekirdeği ileri (68 test). Son aşamaya kadar donduruldu |
| Keşif | Keşif kaydı var. Bölge, yatak, bilgi açılımı yok |

Not: `main`, `feature/simple-intent` dalının 697 commit gerisinde.

Genel: **kod iskeletlerinin çoğu var, sistemler arası bağlantılar ve oyun içi deneme yok.**

---

## 11. Geliştirme sırası

Her fazın sonunda: derleme, otomasyon testi, **oyunda (PIE) kullanıcı denemesi** ve kayıt/yükleme. Bir faz bitmeden sonrakine geçilmez.

| Faz | İş | Bitti ölçütü |
|---|---|---|
| **0. Hazırlık** | Snap dalı main'e girer. Fiziksel montaj dondurulur. Malzeme, eşya, dönüşüm, istasyon, ekin ve olay tabloları veri dosyası olur (`Config/`). | Tüm listeler veriden okunuyor; kodda sabit isim yok |
| **1. Takvim ve hava** | Gün, ay, mevsim, yıl. Aya göre hava. İşaret veren olay yöneticisi (önce kar, yağmur, sel). | Bir yıl hızlandırılmış oynatılınca takvim ve olaylar doğru akıyor, kayıt korunuyor |
| **2. Bireysel hayatta kalma** | Açlık, susuzluk, yorgunluk ve uyku, sıcaklık, ıslaklık, hastalık, yara, bozulma, giysi. | Oyuncu yağmurda ıslanıp üşüyor; bozuk yiyecek ya da kirli su hasta ediyor |
| **3. Ateş, yakıt, barınak** | Yakıt eşyası yanar; ısı; kapalılık; sıva; ıslak odun; kömür. | Yakıt bitince ateş sönüyor, sıvalı barınak daha sıcak |
| **4. Toplama ve dönüşümler** | 30 E-dönüşüm (tarım hariç), 6 istasyon, alet zinciri. | Çakmaktaşından baltaya, kilden pişmiş kaba zincir oyunda yapılabiliyor |
| **Kilometre taşı A — "İlk ay"** | Faz 1–4 birlikte | Oyuncu sıfırdan başlayıp bir ay yaşayabiliyor |
| **5. Arazi ve kazı** | Katmanlar (toprak, kil, kum, taş, çakmaktaşı, aşı); çıkarılan miktar → eşya; kiler, hendek, set, sarnıç; çökme; sınırlar. | Kazılan yer kayıttan sonra aynı, çıkan kil gerçekten eşya, desteksiz oda çöküyor |
| **6. Tarım ve hayvancılık** | 15 aşamalı tarla, 3 ekin, yorulma, tohum saklama; hayvan zinciri; dönüşüm 31–38. | Bir ekin baştan sona (temizlemeden una) oyunda yetiştiriliyor |
| **7. Hareket, keşif, altyapı** | Yük, eğim, zemin; bölge 0–4; ödüller; patika, yol, köprü, ip hattı, merdiven, sal, ileri kamp (basit yerleştirme). | Köprüden önceki ve sonraki dere seferi süresi ölçülebilir şekilde düşüyor |
| **8. Olaylar ve tehditler** | Tüm olaylar ve zincirler; işaretle tetiklenen baskın; yapı hasarı ve onarım; kar yükü, çığ, yangın. | Her olayın işareti görünüyor ve doğru hazırlık zararı önlüyor |
| **9. Denge ve yardımcılar** | Zaman bütçesi ayarı; istasyon ve plan ile zaman kazanımı; yardımcı NPC; bir tam yıl oyun testi. | Bir tam yıl oynanıyor ve kış hazırlığı anlamlı bir zorluk |
| **Kilometre taşı B — "İlk yıl"** | Faz 5–9 birlikte | Bir oyuncu bir yılı baştan sona oynuyor |
| **10. Fiziksel montaj** | 8 F-parçanın tam geometrisi: kütük ailesi, tahta, kazık, taş blok, saz demeti; çentikler, 45° kesim, çapraz kütüklerin iç içe geçmesi. | Sons of the Forest hissi; önceki fazların basit yerleştirmesinin yerine geçiyor (eski yol kalmıyor) |
| **11. Hikâye** | Ayrılmış yerlere izler, kalıntılar, eski yerleşimler. Var olan mekaniklerle aşılır. | Hikâye için yeni mekanik yazılmadı |
| **12. Cila ve demo** | Ses, arayüz, performans, kayıt/yükleme baştan sona; 60–90 dakikalık Steam demosu. | Dışarıdan biri yardımsız oynayabiliyor |
| **Kilometre taşı C — "İlkel çağ tamam"** | Faz 10–12 | Çıkışa hazır ilkel çağ |

Sonraki çağlar (örneğin kil-tuğla, metal): her çağ tek bir yeni malzeme ailesi ekler. O malzeme önceki çağın ürünleriyle açılır. Eski mekanikler silinmez, derinleşir.

---

## 12. Açık sorular

1. Yıl uzunluğu: ay başına 5 gün yeterli mi?
2. Ekin türleri: tahıl, kök bitki ve baklagil için hangi bitkiler?
3. Harita büyüklüğü: 5 bölge tek bir vadide mi?
4. Ölüm ve kaybetme: ölünce ne olur? (Yerleşim kalır mı, eşyalar düşer mi?)
5. Tek oyunculu mu? (Bu plan tek oyunculu varsayıyor.)
6. Yardımcı NPC sayısı ve nasıl kazanıldığı.
7. Merkez yapı (ocak seviyesi gibi) olsun mu, yoksa ilerlemeyi yalnız keşif ve dönüşümler mi açsın?
