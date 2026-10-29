# MyProject3 — Oyuncu Deneyimi ve Oynanış İlerleme Planı

**Belge türü:** Oyun tasarımı / oyuncu deneyimi  
**Durum:** Tasarım hedefi; uygulama veya kabul kanıtı değildir  
**Kapsam:** Oyuncunun dünyaya girişinden gelişmiş, kendi kendine yeten yerleşime uzanan oynanış  
**İlgili kapsam:** M001–M119 mekanik envanteri ve Faz 14 demo/kanıt matrisi

## 1. Oyunun temel vaadi

MyProject3; oyuncunun doğayı gözlemleyerek kaynak topladığı, ham malzemeleri fiziksel olarak işlediği, bu malzemeleri taşıyıp birleştirerek kendi yapılarını kurduğu, araziyi sınırlı ve anlamlı biçimde değiştirdiği ve zaman içinde kendi yaşam alanını geliştirdiği bir hayatta kalma/inşa etme oyunudur.

Oyuncu yalnızca hazır tarifleri çalıştıran veya dünyaya dekor yerleştiren biri değildir. Oyuncu karar verir, malzeme ve zaman harcar, dünyanın durumunu değiştirir ve bu değişikliklerin sonuçlarıyla yaşar.

Oyunun ayırt edici merkezi, **fiziksel malzeme işleme + bağlamsal yerleştirme/snap + destekli inşa + çevresel hayatta kalma** döngüsüdür. Voxel arazi, üretim, tarım/hayvancılık, lojistik, keşif ve baskınlar bu çekirdek etrafında birbirine bağlanır.

## 2. Tasarım sütunları

1. **Dünya sistemleri birbirine bağlıdır.** Yakıt ısı üretir; ısı barınak ve maruziyeti etkiler; hava toprağı ve bitkileri etkiler; yollar/bridgeler taşıma maliyetini değiştirir; yapılar baskınlarda hasar alabilir.
2. **Fiziksel nesnenin durumu gerçektir.** Tam kütük, yarım kütük, çeyrek parça, yontulmuş kütük ve tahta farklı şekil/işlem durumlarıdır. Eşya aynı anda iki yerde veya iki sahipte olamaz.
3. **İnşa, yalnızca görsel yerleştirme değildir.** Parçalar geçerli bağlantı noktalarına bağlanır; destek, dayanım ve yük kuralları yapının güvenliğini etkiler.
4. **Hazırlık, karar ve bedel önemlidir.** Taşıma kapasitesi, araç, yakıt, zaman, hava, mesafe ve barınak seçimi anlamlı kararlar yaratır.
5. **Dünya kalıcıdır.** Kazı, inşa, toplama, üretim, tarım, keşif ve diğer kalıcı sonuçlar kayıt/yükleme sonrası korunmalıdır.
6. **İlk sürüm sade ve test edilebilir olur.** Önce düşük maliyetli geometriler, açık kurallar ve küçük test alanları; nihai modeller, pahalı görseller ve görsel efektler daha sonra.
7. **Oyuncu ne olduğunu anlayabilmelidir.** Bir etkileşimin neden geçerli/geçersiz olduğu, hangi eşyanın tüketildiği ve neyin değiştiği okunabilir geri bildirimle gösterilir.

## 3. Oyuncu deneyimi: beklenen adımlar

### Adım 1 — Dünyaya gir ve çevreyi oku

Oyuncu doğal bir alana girer ve yakındaki ağaçları, taşları, suyu, geçişleri, hava durumunu ve potansiyel tehlikeleri gözlemler. Güvenli kamp yeri ile kaynaklara yakınlık ve çevresel riskler arasında seçim yapar.

**Beklenen sonuç:** Oyuncu ilk kararlarını haritayı ve koşulları okuyarak verir; dünya yalnızca arka plan değildir.

### Adım 2 — Temel ihtiyaçları karşıla

Oyuncu yiyecek/su bulur, temel malzeme toplar, dinlenebileceği bir yer seçer ve koşullara göre geçici barınak veya ateş hazırlar. Yakıt gerçekten tüketilir; ateşin ısısı ve yanma süresi yakıt tanımına bağlıdır.

**Beklenen sonuç:** Oyuncu kaynaklarını yönetir; hava, ıslaklık, sıcaklık, yorgunluk ve barınak seçimi oynanış kararlarına dönüşür.

### Adım 3 — Doğal kaynakları topla

Oyuncu ağaçlardan kütük, zeminden taş ve çubuk gibi malzemeler elde eder. Toplama, uygun nesneye ve eyleme bağlıdır; tek bir kaynak sınırsız çıktı üretemez. Dünyadaki eşya, eldeki eşya ve envanter stoğu tutarlı bir sahiplik akışı izler.

**Beklenen sonuç:** Oyuncu ilk malzeme stokunu oluşturur ve malzemelerin dünyadaki gerçek durumunu öğrenir.

### Adım 4 — Kütüğü işle ve doğru parçayı üret

Oyuncu kütüğü geçerli boyutlara keser, yarar, yontar, oluk açar veya uygun parçalardan tahta üretir. Boy uzunluğu ile kesit/işlem şekli ayrı niteliklerdir. Oluk açma gibi işlemler önkoşullara tabidir; uygun geometri/asset yoksa işlem reddedilir, sahte bir çıktı üretilmez.

**Beklenen sonuç:** İşlenmiş parçanın şekli ve aşaması yeni inşa olanakları açar. İşlemenin hem görsel hem mekanik karşılığı vardır.

### Adım 5 — Nesneleri fiziksel olarak taşı ve yerleştir

Oyuncu kütük ve uygun parçaları alır, taşır, bırakır, döndürür ve yerleştirir. Ağırlık, çarpışma, zemin eğimi ve yerleştirme engelleri önemlidir. Snap çözümleyicisi yakın bağlamdaki geçerli adayları değerlendirir; dolu veya uyumsuz bağlantıyı kabul etmez.

**Beklenen sonuç:** Yerleştirme, rastgele mesh hizalamak değil, anlaşılır kurallarla fiziksel parçaları birleştirmektir.

### Adım 6 — İlk işlevsel yapıyı kur

Oyuncu destekler, kolonlar, yatay kütükler, tabanlar, duvarlar ve çatı gibi parçalarla küçük bir barınak kurar. Bağlantı grafiği ile gerçek fiziksel aktörlerin durumu uyumlu kalır. Bir destek kaldırıldığında bağlı parçaların durumu yeniden değerlendirilir; hasar, onarım, sökme ve malzeme geri kazanımı açık kurallara bağlıdır.

**Beklenen sonuç:** Oyuncu geçici barınaktan daha güvenli bir alana geçer ve kendi yapı kararlarının sonuçlarını görür.

### Adım 7 — Depolama, üretim ve taşıma düzeni kur

Oyuncu malzemeleri düzenler; depolar, raflar ve üretim istasyonları kullanır. Tarifler yalnızca kuyruk kaydı değildir: girdi stoğu ayrılır/tüketilir, çıktı kapasitesi güvence altına alınır ve sonuçlar tek seferde oluşturulur. Üretim başarısızsa malzeme kaybolmaz veya iki kez harcanmaz.

Oyuncu yollar, köprüler, ip hatları veya diğer geçişleri inşa ederek taşıma ve keşif maliyetini azaltabilir.

**Beklenen sonuç:** Oyuncu tek tek toplama/işleme eylemlerinden tekrarlanabilir bir kaynak döngüsüne geçer.

### Adım 8 — Araziyi değiştir ve voxellik mekaniği kullan

Oyuncu kontrollü alanlarda çukur, hendek, siper, ramp veya küçük yer altı alanı oluşturur. Kazı; izin verilen katman, malzeme, derinlik, hacim, araç gücü ve destek koşullarına bağlıdır. Görsel geometri ve çarpışma gerçek anlamda güncellenir. Çıkarılan kaynaklar tanımlı kurallarla elde edilir; değişiklikler kayıt/yüklemede yeniden oluşturulur.

**Beklenen sonuç:** Arazi oyuncunun kararlarıyla değişir ve inşa/ulaşım/hasatla etkileşir; düzenleme sınırsız ve maliyetsiz değildir.

### Adım 9 — Tarım ve hayvancılığı işlet

Oyuncu uygun toprakta tohum eker, sular, büyümeyi izler ve olgunlaşınca hasat eder. Hayvanları uygun yaşam alanında tutar, besler ve ürün toplar. Tür tanımları; zaman, mevsim/hava, nem, bakım, habitat, ürün miktarı ve geçerli eylemler için kurallar içerir.

**Beklenen sonuç:** Oyuncu anlık toplama yerine zaman içinde bakım gerektiren bir gıda/ürün kaynağı geliştirir.

### Adım 10 — Keşfet ve yeni olanaklar aç

Oyuncu uzak bölgeleri, kalıntıları ve çevresel ipuçlarını araştırır. Keşifler kalıcı olabilir; tarif, bilgi, erişim veya etkileşim kilitlerini açabilir. Zorlu bölgelere gitmek yiyecek, ekipman, hava, taşıma yükü ve dönüş planı gerektirebilir. Köprü, yol veya ip hattı daha önce erişilmesi zor bir bölgeyi ulaşılabilir kılabilir.

**Beklenen sonuç:** Keşif, yalnızca rastgele ganimet toplamak değil; dünyayı anlamak ve seçenekleri artırmaktır. Hikâye öncelikle çevre üzerinden aktarılır; zorunlu sinematikler tasarımın temeli değildir.

### Adım 11 — Tehditlere ve çevresel değişimlere karşı koy

Oyuncu daha gelişmiş yapı ve stoklara sahip olduğunda baskınlar, hasar, savunma ve onarım önem kazanır. Düşmanlar geçerli hedef seçmeli, ulaşılabilir rotalar izlemeli ve saldırı etkileri gerçek yapı durumuna uygulanmalıdır. Hava, soğuk/sıcak, ıslaklık ve planlanan doğal olaylar hazırlığı ve hareket seçeneklerini etkiler.

**Beklenen sonuç:** Oyuncunun kurduğu yerleşim yalnızca dekor değil; savunulacak, onarılacak ve çevresel risklere uyarlanacak bir sistemdir.

### Adım 12 — Geçici kamptan gelişen yerleşime ilerle

Oyuncu üretim, depolama, tarım, hayvan alanları, savunma ve ulaşımı birbirine bağlayan kalıcı bir yerleşim kurar. Bir sistemin geliştirilmesi diğer sistemlere yarar sağlar: örneğin daha iyi taşıma altyapısı inşayı kolaylaştırır; daha iyi barınak çevresel baskıyı azaltır; üretim düzeni daha karmaşık yapıları mümkün kılar.

**Beklenen sonuç:** Oyuncu tek bir zorunlu bitişe doğru ilerlemek yerine inşa, keşif, üretim, tarım veya hayatta kalma hedefleri arasında kendi önceliklerini seçebilir.

## 4. Ana oynanış döngüsü

**Gözlemle → ihtiyaç ve fırsat belirle → hazırlan → kaynak topla → malzemeyi işle → taşı/üret/inşa et → sonuçlarla karşılaş → düzeni geliştir → keşif alanını genişlet.**

Döngü hem kısa hem uzun vadeli hedefler üretmelidir. Kısa vadede ateş yakmak veya bir kütüğü işlemek; uzun vadede güvenli bir yerleşim kurmak, üretim zinciri oluşturmak veya uzak bir bölgeye ulaşmak aynı dünya kurallarından beslenir.

## 5. Sistemlerin birbirine bağlanması

| Sistem | Girdi | Dünya üzerindeki sonucu | Bağlantılı sistemler |
|---|---|---|---|
| Kütük/taş/çubuk işleme | Ham malzeme, araç, geçerli işlem | Farklı formda malzeme | Envanter, snap, inşa, üretim |
| Snap ve yapı desteği | Parça, bağlantı noktaları, destek | Yerleşmiş ve desteklenen yapı | Fizik, hasar, baskın, kayıt |
| Voxel kazı | Arazi hücresi, araç, izin ve sınırlar | Değişmiş arazi ve kaynak çıktısı | Envanter, üretim, rota, kayıt |
| Envanter/üretim | Sahip olunan girdiler ve kapasite | Tüketim, rezervasyon ve çıktı | İnşa, tarım, yakıt, lojistik |
| Isı/barınak | Yakıt, ateş/ısı kaynağı, barınak ve kıyafet | Isı ve çevresel maruziyet | Hayatta kalma, yiyecek, hava |
| Tarım/hayvancılık | Zaman, toprak/hava, tohum/yem | Büyüme ve ürün | Envanter, üretim, ihtiyaçlar |
| Yol/köprü/ip hattı | Malzeme ve inşa kuralları | Geçiş ve taşıma kolaylığı | Rota, yük, keşif |
| Baskın/tehdit | Dünya zamanı, hedefler ve tehdit koşulları | Saldırı, hasar, savunma gereksinimi | Yapısal grafik, kayıt, UI |
| Keşif | İpuçları, önkoşullar ve ekipman | Kalıcı bilgi/açılımlar | Tarifler, bölgeler, ilerleme |

Tüm zamanla ilerleyen sistemler aynı yetkili dünya saatini kullanmalıdır. Kaynak sahipliği tek ve açık olmalıdır. Başarısız işlemler kısmi durum bırakmamalı; geri alma da başarısız olursa simülasyon bunu gizlemek yerine hata/fault durumuna geçmelidir.

## 6. İlk oynanabilir sürümde kanıtlanması gereken deneyim

İlk oynanabilir kesit bu 12 adımın tamamını aynı anda sunmak zorunda değildir. Önce aşağıdaki küçük ama uçtan uca döngü güvenilir biçimde çalışmalıdır:

1. Oyuncu kontrollü bir test alanında başlar.
2. Kaynak nesnesiyle etkileşir ve kütük elde eder.
3. Kütüğü geçerli, tanımlı bir işleme durumuna getirir.
4. Kütüğü taşır, bırakır ve doğru konum/yönle yerleştirir.
5. İkinci parçayı uyumlu bir bağlantı noktasına snap eder.
6. Küçük yapı için destek kurallarını görür ve geçersiz bağlantıların reddedildiğini anlayabilir.
7. Malzeme tüketimi ve eşya sahipliği doğru kalır.
8. Dünya kaydedilip yeniden yüklendiğinde yapı ve ilgili nesne durumları korunur.
9. Oyuncu temel ihtiyaçlardan en az birini (ör. yakıt/ısı veya yiyecek) aynı kaynak ve zaman kurallarıyla yönetir.

Bu kesit geçerli olmadan daha büyük dünya veya bütün 119 mekaniğin birlikte çalıştığı bir demo hazır sayılmamalıdır.

## 7. Görsel ve teknik kapsam sınırı

- Önce basit, ölçüleri bilinen placeholder geometriler kullanılır. Yüksek poligonlu model, ayrıntılı malzeme, kesim efektleri, ses ve müzik temel etkileşimler kabul edilmeden önce öncelik değildir.
- Kütük modelinin görsel varyant değiştirmesi, gerçek prosedürel mesh kesimiyle aynı kabul edilmez. Kullanılan her parça boyutu ve işleme şekli için doğru varyant/çarpışma/ölçü tanımlanmalıdır.
- Voxel dünyasında mantıksal hücre değişikliği tek başına yeterli değildir; görünür mesh ve çarpışmanın güncellenmesi gerekir.
- Kodda bir sınıfın veya test tanımının bulunması, özelliğin Unreal içinde oynanabilir ve doğrulanmış olduğu anlamına gelmez.
- Hava efektleri, müzik, animasyon, final assetler, büyük ölçekli dünya üretimi ve yüksek maliyetli fizik sistemleri temel döngü kabulünden sonra aşamalı olarak eklenir.

## 8. 119 mekanikle ilişki ve kabul kuralı

Bu belge **oyuncuya sunulması beklenen deneyimi** tanımlar; M001–M119 listesinin yerini almaz ve hiçbir mekanik durumunu kendiliğinden değiştirmez.

Her mekanik kendi kanonik kimliğine ve bağımlılıklarına bağlı kalmalıdır. Kabul kanıtı, ilgili kaynak sahipliği, geçerli/geçersiz eylemler, sınır durumları, gerekiyorsa kayıt/yükleme ve Unreal derleme/oyun içi sonuçlarını ayrı şekilde göstermelidir. Durumlar en azından şu ayrımları korumalıdır: incelenmedi, kaynakta eşlendi, statik inceleme, Unreal derlemesi geçti, PIE/oyun içi test geçti, kayıt/yükleme testi geçti.

## 9. Oyun tasarımının tek cümlelik özeti

**Oyuncu, fiziksel malzemeleri işleyerek ve doğru biçimde birleştirerek kendi yaşam alanını kurar; araziyi ve ulaşımı değiştirir, kaynak üretir, doğa ve tehditlere hazırlanır, keşif yapar ve dünyada bıraktığı kalıcı sonuçlar sayesinde daha yetenekli ve bağımsız hâle gelir.**
