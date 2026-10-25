# MyProject3 — Ana Uygulama Kontrol Listesi ve Final Demo Planı

**Proje:** MyProject3 / Unreal Engine 5.8  
**Belge rolü:** Bu belge, referans oyunlardan çıkarılan eksiklerin kapatılması için tek ana ilerleme takip noktasıdır.  
**Planın kaynak dalı:** `landscape-builder-plan`  
**Kaynak inceleme temeli:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Son güncelleme:** 2026-10-25  
**Mevcut kanıt seviyesi:** Kaynak/doküman incelemesi. Unreal Engine derlemesi, UHT, Editor, PIE veya oynanış doğrulaması yapıldığı varsayılmaz.

---

## 0. Bu belge nasıl kullanılacak?

- [ ] Her iş başlamadan önce bu belgede ilgili madde ve bağımlılıkları kontrol et.
- [ ] Bir iş gerçekten tamamlandığında yalnızca o işe ait kutuyu işaretle.
- [ ] Bir maddenin alt maddeleri varsa, üst madde ancak bütün gerekli alt maddeler tamamlandığında işaretlenir.
- [ ] Her tamamlanan iş için kaynak dosya/commit/PR, kısa sonuç ve doğrulama kanıtını ilgili iş kaydına ekle.
- [ ] Yalnızca sınıf veya dosya bulunması, doküman yazılması ya da kodun yazılmış olması “oyunda tamamlandı” demek değildir.
- [ ] Kaynak incelemesi, motor derlemesi ve Unreal içi oynanış doğrulamasını ayrı kanıt seviyeleri olarak tut.
- [ ] Kutular sohbet hafızasına bırakılmaz; her tamamlanan işten sonra bu Markdown dosyası güncellenip repoya commit edilir. Böylece yeni sohbet açılsa veya bağlam uzasa da ilerleme dosyada kalır.
- [ ] Kod katmanları ve entegrasyon tamamlanmadan final demo haritasını üretme. Önceden yalnızca mimari ve test senaryosu belgeleri hazırlanabilir.
- [ ] Kullanıcı onayı olmadan PR birleştirme veya ana dalı değiştirme.

### Kanıt durumu tanımları

- **Planlanmadı:** İş ve kabul koşulları henüz belirgin değil.
- **Planlandı:** Bu belgede tanımlı; uygulama tamamlanmış sayılmaz.
- **Kaynakta mevcut/kısmi:** Bir kaynak katmanı mevcut, fakat bütün oyun döngüsü tamamlanmamış olabilir.
- **Kaynak incelemesi tamamlandı:** Kod ve sözleşmeler statik olarak incelendi; motor doğrulaması anlamına gelmez.
- **Motor doğrulandı:** Gerçek UE sürümünde gereken derleme ve çalıştırma kanıtı mevcut.
- **Demo kabul edildi:** Demo senaryosu doğru başlangıç koşullarında çalıştırıldı ve pozitif/negatif sonuçları kayda geçirildi.

Bir kutu yalnızca yanında yazan kabul ölçütü karşılandıktan sonra işaretlenir. Belirsizlik varsa kutu boş kalır.

## 1. Başlangıç: dal, kapsam ve çalışma kuralları

### 1A. Kaynak temeli ve PR zinciri

- [x] H dalını (mechanic-detailing-pass-h-integrated-world-systems) mevcut entegre kaynakların inceleme temeli olarak sabitle.
- [x] H dalındaki M001–M119 kayıtlarını kaynak inceleme/gap raporlarıyla karşılaştır; mevcut, kısmi ve eksik alanları yüksek seviyede eşleştir. (119 kayıt okundu; tam oynanış doğrulaması değil.)
- [ ] `main`, eski faz dalları, A–H dalları ve Landscape plan dalı arasındaki atalık ilişkisini yeniden kontrol et.
- [ ] Yığılmış A–H dallarını bağımsız özellikler gibi sırayla main'e birleştirme; her PR'ın benzersiz değişikliklerini kendi tabanına göre incele.
- [x] PR #16 ve PR #17'nin H dalıyla örtüşmesini kontrol et: G, F'nin bir commit ilerisi; H, G'nin 171 commit ilerisi; PR #17 H'den 13 commit ileride ve beş dokümantasyon dosyasını değiştiriyor. Kod değişikliği olan yeni çalışma ayrı dalda tutuluyor.
- [x] `BRANCH_INTEGRATION_AUDIT.md` commit sayıları ve durumları canlı dal karşılaştırmasına göre güncellendi; yeni düzeltme dalı ve kod kapsamı ayrıca kaydedildi.
- [ ] Her ana iş paketi için amaç, kaynak dosyalar, bağımlılıklar, kabul ölçütleri ve bilinen sınırlamaları kaydet.
- [ ] Mevcut M001–M119 ID'lerini, adlarını ve sahiplik alanlarını değiştirmeden koru.

### 1B. Her mekanik için ortak kabul şablonu

- [ ] Her M-ID için kesin başlangıç durumu ve ön koşulları tanımla.
- [ ] İzin verilen oyuncu eylemini ve beklenen durum geçişini yaz.
- [ ] Fiziksel sonuç varsa aktör/mesh/geometri/çarpışma/konum/miktar çıktısını açıkça belirt.
- [ ] Geçersiz eylem koşullarını ve reddedilince değişmemesi gereken durumları belirt.
- [ ] Tekrar deneme, aynı işlem kimliği, eski revizyon ve çift tıklama durumlarını belirt.
- [ ] İlgili envanter, sahiplik, yapısal grafik, arazi, zaman, üretim ve kayıt etkilerini listele.
- [ ] Kayıt/yükleme davranışını ve gerekli geri yükleme sırasını belirt.
- [ ] Oyuncuya gösterilecek sonucu veya engellenme nedenini tanımla.
- [ ] Pozitif, negatif, sınır durumu, yeniden deneme, entegrasyon ve kayıt/yükleme senaryolarını yaz.
- [ ] Kanıt yerini (dosya/commit/rapor/log/kayıt) belirle.

## 2. P0 — Çekirdek doğruluk ve sahiplik

Bu aşama, daha sonraki bütün sistemlerin dayandığı ortak kuralları kapatır.

### 2A. İşlem ve dünya saati

- [ ] Her dünya değiştiren işlem için işlem kimliği, beklenen revizyon, sonuç kodu ve hata nedeni standardını tamamla.
- [ ] Doğrulama başarısızsa kaynak durumunun değişmediğini güvence altına al.
- [ ] Bir işlem birden çok sistemi değiştiriyorsa ön kontrol/commit/rollback sınırlarını belirle.
- [ ] Mantıksal saati tek yetkili dünya saati olarak kullan; tarım, baskın, üretim, hava ve hayatta kalma için ayrı saatler oluşturma.
- [ ] Sabit zaman adımı sırasını tanımla ve her sistemin bir adımı iki kez çalıştırmasını engelle.
- [x] Rollback başarısızlığı için fail-closed politika eklendi: simülasyon arızalı duruma geçer, yeni sabit adımlar reddedilir, readiness false döner ve açıklama Blueprint üzerinden okunabilir. Kaynak değişikliği; UE derleme/fault-injection henüz yapılmadı.
- [ ] Olayları yalnızca ilgili durum değişikliği commit edildikten sonra yayınla.
- [ ] Bilinmeyen veya geçersiz eşya tanımının ağırlık, kapasite ve sahiplik kontrollerini atlamasını engelle.

### 2B. Eşya kimliği ve sahipliği

- [ ] Her fiziksel kaynak için tekil ve kalıcı kimlik kullan.
- [ ] Dünya, elde tutma, envanter, depo, taşımada olma, üretim tarafından tüketilme ve yok olma durumlarını birbirinden ayır.
- [ ] Aynı eşyanın fiziksel aktör ve envanter yığını olarak iki kez sahiplenilmesini engelle.
- [ ] Bölme, birleştirme, kısmi tüketim ve üretim çıktısı için kimlik/miktar kurallarını tamamla.
- [ ] Kütük, taş ve çubuk aktörlerinin sahiplik defterine bağlanmasını tamamla.
- [ ] Envanter/depo içindeki fiziksel eşyanın dünyaya yeniden aktör olarak çıkarılması için tutarlı bir kural kur.
- [ ] Başarısız toplama, bırakma, üretim veya teslimat işleminde eşya kaybı/çoğaltılması olmadığını kabul senaryolarıyla göster.

### 2C. Envanter ve crafting

- [ ] Bütün eşya kategorileri, tanımlar, ağırlıklar, yığın sınırları ve taşıma limitleri için tek veri kaynağı oluştur.
- [ ] Dünyadan toplama, elde taşıma, envantere koyma, hızlı erişim, depoya aktarma ve bırakma akışlarını tamamla.
- [ ] Tarif girdilerini ve alternatif geçerli tarifleri tanımla.
- [ ] Eksik malzemeleri ve gerekli miktarları oyuncuya göster.
- [ ] Üretim öncesi girdi, çıktı kapasitesi ve alet gereksinimlerini kontrol et.
- [ ] Girdi tüketimi, alet dayanıklılığı, artan malzeme ve çıktı üretimini tutarlı bir işlem olarak ele al.
- [ ] Crafting iptali ve tamamlanan çıktıyı toplama davranışını tanımla.
- [ ] Onarım kiti, koruyucu eşya ve tuzak bileşenleri gibi tarifleri aynı sistem üzerinden bağla.

## 3. Fiziksel kaynak döngüleri — M009–M058

### 3A. Ağaç ve kütük işleme — M009–M028

- [ ] Ağaç kesme ve devrilme durum makinesini tamamla.
- [ ] Ağaç boyutu/kurallarından çıkan kütük miktarı ve boyutlarını tanımla.
- [ ] Tam kütük, taşınan kütük, bırakılan kütük ve yerde serbest kütük durumlarını ayır.
- [ ] Kütüğün yuvarlanma, sürüklenme, itilme ve çarpışma davranışlarını tanımla.
- [ ] Enine kesme çıktılarının boyut ve parça kimliğini belirle.
- [ ] Boyuna yarma sonucunda iki ayrı ve doğru geometriye sahip parça oluştur.
- [ ] Yarım parçayı çeyrekleme işleminin ön koşullarını ve çıktısını tanımla.
- [ ] Yontma/kareleme işleminde kaynak durumu ile çıktı mesh'ini birlikte değiştir.
- [ ] Oluk işlemini yalnızca gerekli boyuna yarma ön koşulundan sonra kabul et.
- [ ] Kanal/tekne biçimi çıktısı için doğru giriş durumu ve fiziksel mesh bulunduğunu doğrula.
- [ ] Tahta üretimini doğru giriş parçası, çıktı miktarı ve geometriyle bağla.
- [ ] Çentik ve geçmeli eklemlerin hangi parça türlerinde uygulanabildiğini tanımla.
- [ ] Kazık biçimlendirme, zemine oturma ve kullanılabilir savunma durumu koşullarını bağla.
- [ ] Özel ve otomatik snap noktalarının kimliklerini koru; yeniden oluşturma özel noktaları silmesin.
- [ ] Yatay, dikey ve çapraz yerleşimin geçerli destek ve dönüşüm kurallarını tamamla.
- [ ] Önizlemede görünen yerleşim ile commit edilen fiziksel dönüşümün aynı olmasını sağla.
- [ ] Taşıyıcı parça kaldırıldığında bağımlı parçaların güvenli sırayla çözülmesini tamamla.
- [ ] Tamir, sökme, hurdaya ayırma ve yakma/geri kazanım çıktılarının eşya sahipliğini bağla.
- [ ] Her kesme eylemi için durum → eylem → ön koşul → çıktı türü → mesh/çarpışma tablosunu tamamla.
- [ ] Özellikle yarım kesilen kütüğün yanlışlıkla tam/düz kütük mesh'ine dönmesi hatasını özel kabul koşulu olarak kapat.

### 3B. Taş ve duvarcılık — M029–M038

- [ ] Taş toplama, elde taşıma, atma ve bırakma akışını tamamla.
- [ ] Ham taş ile şekillendirilmiş yapı taşı türlerini ve işlem çıktılarını tanımla.
- [ ] Taş boyutlandırma/şekillendirme için geçerli alet ve giriş koşullarını belirle.
- [ ] Tek taşın yerleştirme, hizalama ve ankraj işgal kurallarını tamamla.
- [ ] Yatay/dikey istiflemeyi ve köşe bağlantılarını tamamla.
- [ ] Duvar, köşe, kolon, kiriş ve açıklıkları ortak yapı grafiğiyle bağla.
- [ ] Taş desteği, yük, hasar, onarım, çökme ve geri kazanımı tanımla.
- [ ] Taşın toplu taşınması/depolanması için kapasite ve miktar kurallarını bağla.
- [ ] Ahşap-taş karma yapı ve taş savunmalarının bağlantı kurallarını tamamla.

### 3C. Çubuklar, ilkel aletler ve tuzaklar — M039–M048

- [ ] Çubuk toplama, kırma, kesme ve kaynak çıktısı kurallarını tamamla.
- [ ] Sivri çubuk ve çift uçlu kazık üretimini tanımla.
- [ ] İlkel aletler, yakın dövüş silahları, mızrak, yay ve ok tariflerini bağla.
- [ ] Meşale ve basit ışık kaynakları için yakıt/süre/durum kurallarını oluştur.
- [ ] Kazık, çit, bariyer, raf, depolama ve mobilya yerleşimini tamamla.
- [ ] Tuzak parçalarının tamamlanma, zemine sabitlenme, kurulma, tetiklenme ve sıfırlanma koşullarını tanımla.
- [ ] Tuzak tetik çizgisi/hacmi, hedef filtreleri, hasar ve yeniden kurulma davranışını tamamla.
- [ ] Bahçe/hayvan destek yapılarıyla tarım sisteminin bağlantısını kur.
- [ ] Sökme işleminde malzeme geri kazanımı ve sahiplik defteri güncellemesini tamamla.

## 4. Ortak yapı grafiği, destek, hasar ve çökme — M059–M070

- [ ] Kütük, taş ve çubuğun tek yetkili ortak yapısal grafiğini kullanmasını sağla.
- [ ] Her bağlantıda iki uç kimliği, ankraj tipi ve uygunluk kontrolünü tamamla.
- [ ] Yalnızca fiziksel temas/overlap nedeniyle yapısal bağlantı kurulmasını engelle.
- [ ] Zeminden/temelden yüke giden destek yolunu doğrula.
- [ ] Açıklık, çıkıntı, yük kapasitesi ve destek türlerine ilişkin sınırları tanımla.
- [ ] Çoklu destek ilişkilerini veya çoklu destek gerektiren örnekler için açık bir model kur.
- [ ] Bağlantı kaldırılınca iki uçtaki ankraj işgallerini ve ebeveyn/çocuk kayıtlarını temizle.
- [ ] Kapı, pencere, duvar açıklığı, zemin, çatı, rampa, merdiven, köprü, halat geçişi ve korkuluğu aynı yapı modeline bağla.
- [ ] Araç, çarpışma, baskın ve çevresel hasarı tek yetkili bütünlük/hasar hesabından geçir.
- [ ] Destek kaybı ve hasar sonrasında çökme sırasını deterministik yap.
- [ ] Sökme, onarım ve kurtarma sonuçlarını envanter/sahiplik sistemiyle atomik bağla.
- [ ] Güvensiz destek parçasının oyuncu tarafından kaldırılmasını reddet veya tanımlı çökme kuralını uygula.
- [ ] Yapısal ilişkilerin ve ankraj rezervasyonlarının kayıt/yükleme davranışını tamamla.

## 5. Landscape Builder ve dünya arazisi — M071–M080

### 5A. Arazi mimarisi kararı ve profil üretimi

- [ ] UE 5.8'de kullanılabilir Landscape ve Mesh Terrain araçlarını/eklentilerini doğrula.
- [ ] Klasik Landscape, Mesh Terrain ve sınırlı kazı katmanının görevlerini ve sınırlarını yazılı olarak ayır.
- [ ] Deneysel Mesh Terrain özelliklerini doğrulamadan zorunlu üretim bağımlılığı yapma.
- [ ] Seed, profil ID/sürümü, dünya boyutu, yatay/düşey ölçek ve çözünürlük alanlarını tanımla.
- [ ] Dağ, plato, vadi, havza ve oynanabilir alçak alan oranlarını profilde belirt.
- [ ] Ana arazi biçimini üret; gürültüyü yalnızca ayrıntı için kullan.
- [ ] Aynı profil ve seed'in tekrar üretilebilir çıktısını ve manifest/checksum kaydını oluştur.
- [ ] Üretim ayarlarını ve desteklenmeyen değerler için hata davranışını tanımla.

### 5B. Yüzey, biyom, yol ve çevre

- [ ] Landscape temel yüzeyini oluşturma/içe aktarma iş akışını tanımla.
- [ ] Temel yükseklik, bölgesel şekillendirme, iyileştirme, yol/spline ve malzeme katmanlarını ayır.
- [ ] Yükseklik/eğim/bölgesel maskelere bağlı yüzey malzemelerini kur.
- [ ] Korunan inşaat alanları, çiftlik, baskın yaklaşımı ve kazı ceplerini tanımla.
- [ ] Erozyon/yumuşatma işlemini oynanabilir alanları ve yolları bozmadan uygula.
- [ ] Yol, patika, nehir koridoru ve geçiş altyapısının tasarım noktalarını belirle.
- [ ] PCG ile ağaç/kaya/bitki yerleşimi eklenecekse eğim, biyom, yol, görüş ve yasak alan maskelerini bağla.
- [ ] Çevre süslemelerinin tekrar üretilmesi oyuncu yapısını veya arazi kayıtlarını silmesin.
- [ ] World Partition/Data Layers/HLOD ihtiyacını hedef dünya boyutuna göre değerlendir.

### 5C. Sınırlı fiziksel kazı ve dünya durumu

- [ ] Kazılabilir ve korunan bölgeler için tek bir bölge kayıt sistemi kur.
- [ ] Mümkün olmayan derinlik/malzeme/bölge/işlem bütçesi isteklerini değişiklik yapmadan reddet.
- [ ] Kazı hücresi/çevresel state ile fiziksel geometri ve çarpışma arasında deterministik eşleme kur.
- [ ] İlk aşamada yalnızca tanımlanmış kazı ceplerinde sınırlı 3D hücre/chunk veya modüler mesh kullan.
- [ ] Çukur oluşturmanın görünür geometri ve çarpışmaya etkisini tamamla.
- [ ] Yatay kısa tünel ve küçük oda için bağlantılı geometri oluştur.
- [ ] Yeraltı destek/çökme kurallarını yapısal grafikle bağla.
- [ ] Arazi değişikliği tamamlanınca ilgili navigasyon ve yapı desteği verilerini güncelle.
- [ ] Su seviyesi/akışı/yerel taşkın davranışı için veri ve işlem modelini belirle.
- [ ] Toprak nemi, besin ve yüzey akışını tarım girdileriyle bağla.
- [ ] Ateş yayılması, çürüme ve malzeme maruziyeti kurallarını tanımla.
- [ ] Kalıntı ve çevresel ipuçlarını fiziksel dünya aktörleri ve kalıcı keşif sistemiyle bağla.
- [ ] Arazi kayıtlarını sürümlenmiş farklar halinde kaydet ve geri yükleme sırasını tamamla.

## 6. Tarım ve hayvancılık — M081–M090

- [ ] Tek dünya saati üzerinden belirlenmiş sabit adımlı simülasyon sırasını tamamla.
- [ ] Ürün/tohum tanımları, toprak gereksinimleri, mevsim ve ürün aşamalarını veri odaklı yap.
- [ ] Tohum ekimi öncesinde toprak, mevsim, boş alan ve envanter koşullarını kontrol et.
- [ ] Büyüme, sulama, nem, besin ve stres hesabını tamamla.
- [ ] Hasat miktarı ve çıktı kapasitesi kontrolünü girdi/tarla durumuyla tutarlı yap.
- [ ] Tarla/ürün durumunu kayıttan geri yükle; ürünleri ikinci kez büyütme veya hasat etme.
- [ ] Hayvan türü, açlık, sağlık, büyüme ve bakım durumlarını tamamla.
- [ ] Yem tüketimini envanter/depo sahipliğiyle bağla.
- [ ] Habitat kapasitesi, hayvan hareketi ve üreme koşullarını tanımla.
- [ ] Süt/yumurta/yün ve diğer ürünleri gerçek eşya kayıtlarına aktar.
- [ ] Gübre ve yan ürünleri toprak besin döngüsüne bağla.
- [ ] Arazi veya destek yapısı değiştiğinde tarla/hayvan alanının geçerliliğini kontrol et.

## 7. Baskın, tehdit ve düşman davranışı — M091–M100

- [ ] Baskın çizelgesini ortak dünya saati ve deterministik seed ile bağla.
- [ ] Oyuncu/dünya bağlamına göre zorluk değerlendirmesinin girdilerini açıkça tanımla.
- [ ] Dalga ve düşman bütçesini sınırlı ve ayarlanabilir tut.
- [ ] Geçerli ve yasak düşman oluşturma bölgelerini belirle.
- [ ] Gerçek hedef seçimi, rota/navigasyon, takip ve saldırı davranışını tamamla.
- [ ] Düşmanların engelleri aşma veya başka bir geçiş yolu seçme davranışını tanımla.
- [ ] Yapı hasarını ortak yapısal hasar yoluna bağla.
- [ ] Tuzak, arazi ve savunma yapılarının baskınla etkileşimini bağla.
- [ ] Başarısız/iptal edilen/engellenen baskın için açık sonuç ve toparlanma davranışı kur.
- [ ] Yerleşim gürültüsü, stok ve savunma miktarının baskın baskısına etkisini açık ve ayarlanabilir yap.
- [ ] Kayıt/yükleme ve yeniden deneme sırasında dalga, hasar ve ödül çoğaltılmasını engelle.

## 8. Lojistik, taşıma ve üretim — M101–M106

- [ ] Depo, raf, araba/kızak ve diğer taşıma nesnelerinin kapasite/kabul kurallarını tanımla.
- [ ] Kaynaktan hedefe aktarımda kaynak miktarı ve hedef kapasitesini işlemden önce kontrol et.
- [ ] Kısmi transfer davranışını açıkça tanımla; sessiz eşya kaybı olmasın.
- [ ] Halat hattı/zipline ile taşıma isteklerini rota ve eşya sahipliğiyle bağla.
- [ ] Mantıksal kargo ilerlemesini gerçek fiziksel hareket/varış gösterimiyle bağla.
- [ ] Rota maliyetlerini yüzey, eğim, yol, köprü, halat, yük, hava ve tehdit girdileri üzerinden tasarla.
- [ ] Üretim tarifi girdileri, çıktıları, yan ürünleri, istasyon etiketi ve süreyi tanımlasın.
- [ ] Kuyruk uzunluğu, kaynak rezervasyonu ve çıktı kapasitesi sınırlarını doğrula.
- [ ] Üretim işi başlatma/tamamlama sırasında stok ve iş durumunu tutarlı değiştir.
- [ ] Bloke nedenlerini (girdi, istasyon, yakıt, kapasite, rota) açıkça raporla.
- [ ] Yakıt, ısı ve yemek pişirme durumunu ortak dünya saati ve hayatta kalma ile bağla.
- [ ] Ham kütükten işlenmiş parçaya/tahtaya giden en az bir gerçek üretim hattını tamamla.
- [ ] Üretilen çıktıyı eşya sahipliği ve depo/envanter sistemine kayıp/çoğalma olmadan aktar.
- [ ] Kargo, üretim işi, tarif, stok ve sahiplik kayıtlarının yeniden yüklemede tutarlı kalmasını sağla.

## 9. Hayatta kalma ve keşif — M107–M119 (M109 kayıt/entegrasyon kapısıdır)

- [ ] Sağlık, açlık, susuzluk, dayanıklılık, yorgunluk, sıcaklık ve maruziyetin tek yetkili durum sahipliğini tanımla.
- [ ] Hava ve mevsim sistemini ortak dünya saatinden besle.
- [ ] Soğuk/sıcak/ıslaklık, kıyafet yalıtımı ve barınak korumasını tek maruziyet çözümleyicisinde birleştir.
- [ ] Uyku ve dinlenme eylemlerini gerçek zaman ilerlemesi ve toparlanmayla bağla.
- [ ] Yaralanma, hastalık ve zehirlenme durumları ile etkilerini tanımla.
- [ ] Yiyecek kalitesi ve bozulmayı geçen mantıksal zamana bağla.
- [ ] Ateş, yakıt, ısı ve yemek pişirme sistemini tamamla.
- [ ] Koruyucu ekipman/kıyafet ile envanter arasında tekil sahiplik bağlantısı kur.
- [ ] Oyuncunun kaynak, dayanıklılık ve depolama durumunu görebileceği geri bildirim ekle.
- [ ] Keşfedilebilir kalıntı/ipucu aktörlerini kalıcı keşif kayıtlarına bağla.
- [ ] Keşif sonrasında hangi dünya bağlamının açıldığı veya değiştiğini oyuncuya göster.
- [ ] Uygun hazırlık ile zorlu koşulların aşılabildiği kuralları ve ayar aralıklarını belirle.
- [ ] Keşif, hayatta kalma ve çevre durumlarının kayıt/yükleme sonrası korunduğunu tanımla.

## 10. Tam dünya kaydı/yükleme ve entegrasyon — M007, M076, M109 ve diğer kayıt gerektiren işler

- [ ] Kayıt manifesti, şema sürümü ve geriye dönük uyumluluk politikasını tanımla.
- [ ] Eski şema sürümü için göç (migration) veya güvenli reddetme davranışını belirle.
- [ ] Geri yükleme sırasını şu bağımlılıklara göre tanımla: dünya/kimlik/saat → temel arazi → kazı farkları → fiziksel aktörler → yapısal grafik/ankrajlar → eşya sahipliği/envanter/depolar → tarım/üretim/kargo → baskın/hayatta kalma/keşif → türetilen çarpışma/navigasyon.
- [ ] Fiziksel aktörlerin kimlik, sınıf/tür, dönüşüm ve alan durumlarını kaydet/geri kur.
- [ ] Yapısal grafik bağlantıları ve ankraj rezervasyonlarını güvenli geri yükle.
- [ ] Kazılmış arazi geometrisi ve çarpışma değişikliklerini kayıt farklarından yeniden kur.
- [ ] Elde tutulan nesneler gibi temsil edilmeyen geçici durumların kayıt öncesi güvenli şekilde çözülmesini veya açıkça reddedilmesini sağla.
- [ ] Envanter, depo, kargo, üretim, tarım ve kaynak sahipliği çapraz referanslarını doğrula.
- [ ] Eksik/bozuk referansları güvenli reddetme veya belgeli kurtarma politikasıyla ele al.
- [ ] Kayıt sırasında veya yükleme esnasında kısmi dünya durumunun oynanışa açılmasını engelle.
- [ ] Aynı kaydı tekrar yüklemek eşya, hasat, baskın, üretim çıktısı veya keşif çoğaltmasın.
- [ ] Bütün sistemler için kayıt/geri yükleme kabul senaryosunu yaz.

## 11. M001–M119 bireysel kapanış izleyicisi

Aşağıdaki bir ID ancak kendi kabul ölçütleri, çapraz sistem bağımlılıkları, hata/tekrar denemesi ve gerekiyorsa kayıt davranışı kanıtlandığında işaretlenir. Bir sistem dosyasının varlığı kutuyu işaretlemek için yeterli değildir.

### Çekirdek dünya sözleşmeleri
- [ ] M001 — Fiziksel dünya nesneleri için kalıcı kimlik
- [ ] M002 — Nesne yaşam döngüsünde tekil durum
- [ ] M003 — Atomik dünya değiştiren işlem
- [ ] M004 — Yapılandırılmış işlem sonucu ve hata nedeni
- [ ] M005 — Commit edilmiş alan olayları
- [ ] M006 — Deterministik dünya saati ve rastgele seed
- [ ] M007 — Sürümlü kayıt/yükleme ve migration sınırı
- [ ] M008 — Ortak hayatta kalma durumu

### Kütük/ahşap işleme
- [ ] M009 — Ağaç kesme
- [ ] M010 — Ağaç boyutundan kütük üretimi
- [ ] M011 — Serbest kütüğü toplama/taşıma
- [ ] M012 — Kütüğü bırakma/hızlı bırakma
- [ ] M013 — Serbest kütüğü yuvarlama/sürükleme/itme
- [ ] M014 — Kütük çarpışması ve darbe hasarı
- [ ] M015 — Kütüğü geçerli boyda enine kesme
- [ ] M016 — Kütüğü boyuna yarma
- [ ] M017 — Yarım kütüğü çeyrekleme
- [ ] M018 — Kütüğü yontma/kareleme
- [ ] M019 — Yarmadan sonra boyuna oluk açma
- [ ] M020 — Kanal/tekne biçimi oluşturma
- [ ] M021 — Yarım kütükten tahta üretme
- [ ] M022 — Çentik ve geçmeli bağlantı kesme
- [ ] M023 — Zemine oturan kazık biçimlendirme
- [ ] M024 — Snap ankrajlarını oluşturma/doğrulama
- [ ] M025 — Kütükleri yatay/dikey/çapraz yerleştirme
- [ ] M026 — Ebeveyn/çocuk yapı bağlantılarını koruma
- [ ] M027 — Destek kaldırma ve çökme çözümleme
- [ ] M028 — Ahşap tamiri, geri kazanımı ve yakma

### Taş/duvarcılık
- [ ] M029 — Taş toplama, taşıma ve fırlatma
- [ ] M030 — Ham taşı yapı parçasına dönüştürme
- [ ] M031 — Tek taş yerleştirme ve hizalama
- [ ] M032 — Taşları yatay/dikey istifleme
- [ ] M033 — Taş duvar ve köşe oluşturma
- [ ] M034 — Taş kolon, kiriş ve açıklık oluşturma
- [ ] M035 — Taş desteği, denge ve çökme
- [ ] M036 — Taş yapıya hasar, tamir ve geri kazanım
- [ ] M037 — Taşı toplu depolama/taşıma
- [ ] M038 — Ahşap-taş karma yapı ve taş savunma bağlantıları

### Çubuklar/ilkel araçlar
- [ ] M039 — Çubuk toplama ve kırma
- [ ] M040 — Çubuğu sivriltme/çift uçlu kazık üretme
- [ ] M041 — İlkel alet/yakın dövüş silahı üretme
- [ ] M042 — Mızrak, yay ve ok üretme
- [ ] M043 — Meşale ve basit ışık kaynağı
- [ ] M044 — Kazık, çit ve bariyer oluşturma
- [ ] M045 — İlkel tuzak ve tetik hattı
- [ ] M046 — Çubuk deposu, raf ve mobilya
- [ ] M047 — Basit bahçe/hayvan destek yapıları
- [ ] M048 — Çubuk yapısını sökme ve malzeme geri alma

### Envanter/crafting
- [ ] M049 — Dünya eşyasını envantere alma
- [ ] M050 — Yığın ve taşıma sınırları
- [ ] M051 — Eşya kategorisi/özellik/hızlı erişim
- [ ] M052 — Malzemeleri crafting alanına koyma
- [ ] M053 — Tarif ve alternatif tarifleri çözme
- [ ] M054 — Eksik malzemeleri/miktarları gösterme
- [ ] M055 — Crafting onayı ve atomik tüketim
- [ ] M056 — Aleti koruma ve kullanılmamış malzemeyi geri verme
- [ ] M057 — Crafting iptali ve sonucu toplama
- [ ] M058 — Tamir kiti, koruma ve tuzak bileşenleri

### Yapı grafiği/hasar
- [ ] M059 — Tek yetkili yapısal grafik
- [ ] M060 — Zeminden yüke destek yolu
- [ ] M061 — Açıklık, çıkıntı ve yük sınırları
- [ ] M062 — Yapısal bağlantı hasar eşikleri
- [ ] M063 — Bağımlı parçaları kararlı sırayla çökertme
- [ ] M064 — Kapı, pencere ve duvar açıklıkları
- [ ] M065 — Zemin, çatı, rampa ve merdiven
- [ ] M066 — Köprü, halat geçişi ve yüksek yürüyüş yolu
- [ ] M067 — Kapı, çit, korkuluk ve savunma duvarı
- [ ] M068 — Durum çoğaltmadan hasar/tamir
- [ ] M069 — Yapı sökümü ve kaynak geri kazanımı
- [ ] M070 — Taşıyıcı parçanın güvensiz alınmasını engelleme

### Arazi/çevre
- [ ] M071 — Kalıcı arazi parçaları/chunk düzeni
- [ ] M072 — Yalnızca tanımlanmış arazi katmanlarını düzenleme
- [ ] M073 — Geometri/çarpışma içeren çukur kazma
- [ ] M074 — Yatay tünel ve küçük oda kazma
- [ ] M075 — Yeraltında desteksiz çökme davranışı
- [ ] M076 — Arazi değişikliklerini kaydetme/yükleme
- [ ] M077 — Su seviyesi, akış ve yerel taşkın
- [ ] M078 — Toprak nemi, besin ve yüzey akışı
- [ ] M079 — Ateş yayılması, çürüme ve maruziyet
- [ ] M080 — Çevresel kalıntı ve ipucu keşfi

### Tarım/hayvancılık
- [ ] M081 — Deterministik dünya zamanı adımları
- [ ] M082 — Tohum ekme ve toprak/mevsim kontrolü
- [ ] M083 — Ürün büyüme aşamaları
- [ ] M084 — Sulama, besin ve bitki stresi
- [ ] M085 — Hasat ve ürün miktarı
- [ ] M086 — Hayvan besleme ve bakımı
- [ ] M087 — Hayvan büyüme ve sağlık ilerlemesi
- [ ] M088 — Süt/yumurta/yün ve diğer yapılandırılmış ürünler
- [ ] M089 — Habitat kapasitesine bağlı üreme
- [ ] M090 — Yem, gübre ve toprak besin döngüsü

### Baskın/tehdit
- [ ] M091 — Tehdit olaylarını planlama ve koşullandırma
- [ ] M092 — Oyuncu/dünya durumuna göre zorluk
- [ ] M093 — Düşman grubu/dalga/kaynak bütçesi
- [ ] M094 — Düşman oluşturma kapasitesi ve kuyruk sınırı
- [ ] M095 — Geçerli düşman doğma noktaları
- [ ] M096 — Hedef seçimi ve navigasyon yolu
- [ ] M097 — Saldırı, alternatif rota ve yapı hasarı
- [ ] M098 — Dalga durum makinesi
- [ ] M099 — Baskın iptali, kesilmesi ve toparlanma
- [ ] M100 — Yerleşim gürültüsü/stok/savunmaya göre baskı

### Lojistik/üretim/hayatta kalma/keşif
- [ ] M101 — Araba/kızak/depo rafına yükleme/boşaltma
- [ ] M102 — Halat hattı/zipline ile malzeme taşıma
- [ ] M103 — İlkel testere ve üretim zinciri
- [ ] M104 — Üretim kuyruğu ve engellenme nedeni
- [ ] M105 — Yakıt, ısı ve yemek pişirme
- [ ] M106 — Yol/köprü ile rota maliyetini azaltma
- [ ] M107 — Barınak, sıcaklık ve günlük hayatta kalma
- [ ] M108 — Kaynak, dayanıklılık ve depo durumunu gösterme
- [ ] M109 — Dünya/üretim genelinde entegre kayıt ve yükleme
- [ ] M110 — Hava ve mevsim geçişleri
- [ ] M111 — Soğuk, sıcak ve ıslaklık maruziyeti
- [ ] M112 — Açlık ve susuzluk tüketimi
- [ ] M113 — Dinlenme/aktivite ile dayanıklılık toparlanması
- [ ] M114 — Uyku ve yorgunluk etkileri
- [ ] M115 — Yaralanma, hastalık ve zehirlenme
- [ ] M116 — Zamanla yiyecek kalitesi/bozulma
- [ ] M117 — Kıyafet yalıtımı ve koruyucu ekipman
- [ ] M118 — Barınak kalitesinin maruziyete etkisi
- [ ] M119 — Keşifleri kalıcı kılma ve dünya bağlamı açma

## 12. Yeni mekanik ekleme kuralı — M120+

- [ ] Yeni bir özellik eklemeden önce M001–M119 içinde aynı davranışın olup olmadığını kontrol et.
- [ ] Mevcut davranışın ayrıntısıysa yeni ID yerine alt gereksinim (ör. M106-R01) oluştur.
- [ ] Gerçekten yeni oyuncu davranışıysa adayın amacı, sahibi, bağımlılıkları, hata durumları, kayıt kuralı ve demo senaryosunu yaz.
- [ ] Onaylanan yeni ID'yi M119 sonrasına ekle; var olan ID'leri yeniden numaralandırma.
- [ ] CSV/JSON/Markdown registry, şema, faz planı ve demo kabul haritasını birlikte güncelle.
- [ ] Landscape Builder'ın heightmap, erozyon, profil, maskeler, spline ve PCG editör araçlarını otomatik olarak yeni oynanış mekaniği sayma.

Adaylar yalnızca inceleme konusudur, henüz kayıtlı mekanik değildir:
- [ ] M120 adayı — Eğim/yüzeye bağlı hareket ve taşıma maliyeti; M106/M111 ile çakışma incelemesi.
- [ ] M121 adayı — Eğim/zemine göre yapı yeri uygunluğu; M060 ile çakışma incelemesi.
- [ ] M122 adayı — Jeoloji katmanına bağlı kaynak yatağı çıkarımı.
- [ ] M123 adayı — Yer üstünde heyelan/kaya düşmesi; M075 yeraltı çökmesinden ayrım.

## 13. Final demo — 119 mekaniğin aynı dünyada birlikte çalıştığını kanıtla

**Temel kural:** Final demo, 119 mekanik için 119 ayrı vitrinden veya birbirinden kopuk test adalarından oluşmayacak. Tek bir küçük, kalıcı ve birbirine bağlı dünyada, mekaniklerin birbirlerinin durumunu nasıl değiştirdiğini gösterecek. Yedi bölge yalnızca okunaklı yerleşim/işlev alanlarıdır; yedi bağımsız simülasyon, yedi ayrı kaynak sistemi veya birbirinden kopuk sahneler değildir.

Demo aynı anda iki şeyi kanıtlamalı:

1. **Uçtan uca oynanış döngüsü:** Oyuncu dünyadan kaynak alır, işler, taşır, inşa eder, yaşar, üretim yapar, savunur, çevreyi değiştirir ve keşif yapar. Bir sistemin çıktısı sonraki sistemin gerçek girdisi olur.
2. **119 ID'nin kabul kapsamı:** Her M-ID için ayrı ayrı tanımlı pozitif/negatif durum, ön koşul, beklenen sonuç, tekrar deneme, çapraz sistem etkisi ve kayıt beklentisi kanıtlanır. Bir özelliği bütün dünyayla bağlamak, o ID'nin özel sınır ve hata koşullarının kontrol edilmesi gereğini ortadan kaldırmaz.

Bir ekran görüntüsü veya mekanik adını gösteren tabela, entegrasyon kanıtı değildir. Kanıt; aynı kimliğe ve kayda bağlı durumların gerçekten değiştiğini, sonucu sonraki sistemin tükettiğini ve başarısız işlemin tutarsızlık bırakmadığını göstermelidir.

### 13A. Demo öncesi kapı

- [ ] Bölüm 2–10'daki gerekli kod katmanları ve birbirleriyle bağlantıları tamamlandı.
- [ ] M001–M119 için açık ön koşullar, durum geçişleri, fiziksel/mantıksal çıktılar, hata kuralları ve kayıt beklentileri yazıldı.
- [ ] Her mekanik birincil sahibiyle ve kullandığı ortak sistemlerle eşleştirildi; aynı gerçeği yöneten çift sahipli sistem kalmadı.
- [ ] Ortak saat, eşya sahipliği, işlem kimliği/revizyon, yapısal grafik, arazi durumu, navigasyon ve kayıt/yükleme sözleşmeleri kaynak incelemesinden geçti.
- [ ] Her çapraz sistem bağlantısı için kaynak sistemi → olay/işlem → tüketen sistem → beklenen gözlenebilir sonuç tanımlandı.
- [ ] Demo dünyasının seed'i, profil sürümü, başlangıç kaynakları, başlangıç zamanı ve test reset yöntemi belirlendi.
- [ ] Tek dünyadaki alanlar, yollar, köprüler/halat geçişleri, çiftlik, atölye, savunma ve keşif noktaları birbirine anlamlı biçimde bağlandı.
- [ ] Hangi işlerin yalnızca kaynak incelemesi, hangilerinin Unreal motoru ve gerçek oynanış kanıtı gerektirdiği ayrıldı.
- [ ] Kullanılacak mesh/aktör/Blueprint/config/Editor varlıkları listelendi; sırf sahnede görünsün diye gereksiz varlık eklenmedi.

### 13B. Ana entegre oynanış senaryosu — tek bir sebep-sonuç zinciri

Aşağıdaki adımlar tek bir dünya kaydı içinde ilerlemeli. Bölge değiştirildiğinde veya oyuncu atölyeye döndüğünde, önceki yerde oluşan kaynak/yapı/arazi/zaman durumu kaybolmamalı.

#### Bölüm A — Dünya üretimi, keşif ve ilk kaynaklar

- [ ] Sabit profil ve seed ile dış arazi oluşturuldu; dağ/vadi/eğim, yüzey malzemeleri, korunmuş oyun alanları ve izinli kazı alanları aynı dünya üzerinde tanımlandı.
- [ ] Oyuncu arazi ve çevresel ipuçlarını keşfedebildi; keşif kaydı dünya bağlamıyla ilişkilendirildi.
- [ ] Oyuncu ağaç, kütük, taş ve çubuk kaynaklarına ulaştı; her kaynak kalıcı ve tekil kimlik aldı.
- [ ] Ağaçtan çıkan kütüklerin miktarı/boyutu tanımlı kurallara uydu; toplama/taşıma/bırakma fiziksel aktörü ile eşya sahipliği aynı durumu gösterdi.
- [ ] Geçersiz toplama/işleme işlemleri kaynak veya envanter durumunu değiştirmeden reddedildi.

#### Bölüm B — Malzeme işleme, envanter ve yapı kurma

- [ ] Oyuncu gerçek kaynakları atölyeye taşıdı; aynı kaynak dünya, envanter, depo ve kargo içinde çoğalmadı.
- [ ] Tam kütükten enine kesme, boyuna yarma ve gereken diğer dönüşümler doğru çıktı parçasını ve geometriyi üretti.
- [ ] Oluk, kanal, tahta ve çentik gibi işlemler gereken giriş durumu olmadan reddedildi; başarılı işlemde parça tipi/mesh/çarpışma birlikte değişti.
- [ ] İşlenmiş parçalar crafting/yapı tariflerinde gerçekten tüketildi ve doğru yapı bileşenlerine dönüştü.
- [ ] Temel, ankrajlar, destekler ve karma taş-ahşap-çubuk parçaları ortak yapısal grafiğe bağlandı.
- [ ] Yalnızca birbirine değen iki parça yapısal bağ kazanmadı; zeminden yük taşıyan parçaya geçerli destek yolu kuruldu.
- [ ] Güvensiz destek kaldırma ya reddedildi ya da bağımlı parçalar, kaynak sahipliği ve çarpışma dâhil tanımlı sırada çözüldü.
- [ ] Yapı onarımı/sökümü ve kaynak geri kazanımı envanter, dünya aktörü ve grafik durumuyla tutarlı kaldı.

#### Bölüm C — Arazi değişimi, çiftlik ve çevre

- [ ] Oyuncu yalnızca belirlenmiş bir kazı cebinde çukur veya kısa tünel/oda oluşturabildi; korunan alanda veya izin dışı derinlikte değişiklik olmadı.
- [ ] Kazı aynı anda mantıksal arazi kaydı, görünür geometri ve çarpışmayla eşleşti.
- [ ] Kazı mevcut temel/yapı desteğini etkilediyse yapısal sistem bilgilendirildi; güvenli olmayan değişiklik reddedildi veya tanımlı sonucu uygulandı.
- [ ] Yüzey/eğim/altyapı değişikliği ilgili navigasyon ve rota maliyeti tüketicilerine bildirildi.
- [ ] Oyuncu aynı dünyadaki tarla alanını hazırlayıp tohum ekti; ortak saat ilerledikçe ürün durumları değişti.
- [ ] Su/nem/besin/mevsim koşulları ürün büyümesini etkiledi; hasat edilen ürün gerçek envanter/depo kaydına aktarıldı.
- [ ] Hayvan yemi stoktan tüketildi; hayvan bakımı/sağlığı ve ürün çıktıları ortak zamanla ilerledi; üretilen malzeme sahiplik defterinde tek kez oluştu.
- [ ] Yağış/nem/ateş/çürüme gibi uygulanmış çevre kuralları arazi, kaynak veya hayatta kalma tüketicileriyle gerçek bir sonuca bağlandı. Henüz uygulanmamış çevre sistemleri demo kapsamında varmış gibi gösterilmedi.

#### Bölüm D — Lojistik ve üretim zinciri

- [ ] Oyuncu ham ve işlenmiş kaynakları depoladı; depo kapasitesi ve kaynak sahipliği korundu.
- [ ] Oyuncu en az bir gerçek taşıma altyapısını (ör. yol/köprü veya halat/zipline) kullanarak kaynağı kaynak noktasından atölye/depo/yerleşime taşıdı.
- [ ] Rota kararı ve maliyeti yalnızca elle yazılmış bir düğüm yolu değil, mevcut arazi/altyapı durumuyla tutarlı oldu.
- [ ] Taşıma sırasında eşya tek bir `InTransit` sahibi altında kaldı; başarılı teslimat doğru hedefe aktardı.
- [ ] En az bir üretim istasyonu gerçek tarif girdilerini tüketip tanımlı çıktıyı üretti.
- [ ] Üretim kuyruğu, girdi/istasyon/yakıt/kapasite eksikliğinde açık bir bloke nedeni gösterdi ve başarısız başlatmada kaynakları beklenmedik şekilde tüketmedi.
- [ ] Üretilen çıktı yeniden depo/envanter/taşıma/yapı döngüsünde kullanılabildi; yalnızca görsel bir üretim efekti olarak kalmadı.
- [ ] Yol/bağlantı kapatıldığında veya hedef dolduğunda teslimat/üretim durumu açıklanabilir kaldı ve kaynaklar kaybolmadı.

#### Bölüm E — Hayatta kalma, yerleşim baskısı ve baskın

- [ ] Oyuncu açık hava maruziyeti ile barınak/kıyafet koruması arasında ölçülebilir fark gördü.
- [ ] Açlık, susuzluk, dayanıklılık, yorgunluk, uyku/dinlenme ve yaralanma gibi uygulanmış ihtiyaçlar aynı hayatta kalma durumunda ilerledi.
- [ ] Yiyecek, yakıt, ateş ve yemek pişirme uygulanmışsa gerçek envanter/üretim/depo durumunu değiştirdi; bozulma ortak saatle ilerledi.
- [ ] Yerleşimin yapıları, stokları, gürültüsü ve savunması tehdit sistemine açık, ayarlanabilir girdiler sağladı.
- [ ] Baskın ortak saatte planlandı; uyarı, geçerli doğma konumu, hedef seçimi, yol bulma, saldırı ve sonuç durumları birbirine bağlandı.
- [ ] Düşman hasarı yapısal grafik üzerinden uygulandı; savunma/tuzak, yapı bütünlüğü, çökme, tamir ve kaynak geri kazanımı aynı durumu kullandı.
- [ ] Baskın sırasında bir yol/köprü/kapı veya destek zarar gördüğünde lojistik/erişim ve yapı sistemleri bu değişimi dikkate aldı.
- [ ] Baskın engellendiğinde, kesildiğinde, geri püskürtüldüğünde veya başarısız olduğunda kaynak/ödül/dalga durumu tekrar denemeyle çoğalmadı.
- [ ] Oyuncu kaynak, ekipman, hazırlık veya rota seçimini kullanarak bu baskından toparlanabildi; zorluk kaçınılmaz hasara indirgenmedi.

#### Bölüm F — Kayıt/yükleme ve aynı dünyada devam

- [ ] Oyuncu dünya durumunda anlamlı değişiklikler yaptıktan sonra kayıt aldı.
- [ ] Kayıtta profil/seed/saat, arazi ve kazı farkları, fiziksel aktörler/dönüşümler, eşya sahipliği, envanter/depolar, yapısal grafik/ankrajlar, tarım/hayvan, kargo/üretim, baskın, hayatta kalma ve keşif alanlarının kapsamı açıkça doğrulandı.
- [ ] Yükleme sonrasında aynı dünya durumu yeniden kuruldu; eşya, hasat, üretim çıktısı, düşman dalgası veya keşif çoğalmadı.
- [ ] Türetilmiş görsel, çarpışma ve navigasyon verileri geri yüklenen yetkili duruma göre yeniden kuruldu.
- [ ] Yükleme sonrasında oyuncu yeni bir eyleme devam edebildi ve sonraki sistemler önceki durumları tüketebildi.
- [ ] Aynı senaryo aynı seed/başlangıç durumu ile yeniden başlatıldığında deterministik sistemlerin sonuçları karşılaştırılabilir oldu.

### 13C. Zorunlu sistemler arası bağlantı matrisi

Bu matrisin her satırı ayrı kabul kriteridir. Bir sistemin kaynak dosyasının bulunması, satırın tamamlandığı anlamına gelmez.

| Kaynak / olay | Bağlantı kurulacak sistem | Demo ile kanıtlanacak sonuç |
|---|---|---|
| Ağaç kesme ve kütük işleme | Eşya sahipliği, envanter, crafting | Tekil kaynak doğru işlenir; miktar ve fiziksel çıktı tutarlıdır |
| Ankraj/temel/bağlantı | Yapısal grafik, yük, hasar | Yerleşim geçerliliği destek yoluna bağlıdır |
| Kazı/zemin değişimi | Çarpışma, navigasyon, destek, kayıt | Görünür kazı, fiziksel zemin ve destek durumu aynı sonucu yansıtır |
| Ortak dünya saati | Tarım, hayvanlar, üretim, hava, baskın, hayatta kalma | Aynı zaman adımı tüm ilgili sistemleri tutarlı ilerletir |
| Hasat/hayvan ürünü | Eşya sahipliği, depo, üretim, yemek | Çıktı bir kez oluşur ve sonraki sistem gerçek girdiyi tüketir |
| Yol/köprü/halat değişimi | Lojistik, kargo, üretim | Rota erişimi/maliyeti ve teslimat sonucu değişir |
| Hava/toprak koşulu | Tarım, maruziyet, ateş/malzeme | Ortak çevresel durum ilgili sistemlerde gözlenebilir etki yaratır |
| Barınak/kıyafet/ateş | Hayatta kalma, envanter, yakıt ve yemek | Koruma, tüketim ve toparlanma aynı duruma bağlanır |
| Stok/gürültü/savunma | Tehdit/baskın, yapısal hasar | Yerleşim durumu baskın kararını etkiler; hasar ortak yapısal yolu kullanır |
| Yapı hasarı/çökme | Envanter/kurtarma, rota/erişim, kayıt | Yıkım sonrası kaynak ve erişim durumu tutarlıdır |
| Keşif/kalıntı | Kalıcı dünya bağlamı, kayıt/yükleme | Keşif görünür içerik veya yeni bağlam açar ve kalıcıdır |
| Kayıt/yükleme | Bütün yetkili sistemler | Dünya yalnızca mantıksal değerleri değil, ilgili fiziksel sonuçları da tutarlı geri kurar |

- [ ] Matrisin bütün satırları için kaynak kod/kontrat referansı yazıldı.
- [ ] Her satırda en az bir pozitif entegrasyon senaryosu var.
- [ ] Uygun bağlantılarda en az bir negatif veya bozulma senaryosu var.
- [ ] Başarısızlık/rollback durumunda hem kaynak hem tüketen sistemin beklenen son durumu belirtildi.
- [ ] Tüketici sistemlerin aynı kaynağı bağımsız olarak ikinci kez değiştirmesi engellendi.

### 13D. Tek dünya, yedi okunaklı alan

Yedi alan **aynı küçük dünyada birbirine fiziksel ve işlevsel olarak bağlı** olmalı. Oyuncunun kaynak alanında ürettiği kütük inşaat alanında kullanılabilmeli; kazı yapı desteğini etkileyebilmeli; çiftlik ürünleri depoya ve üretime gidebilmeli; yol/halat rotası lojistikte kullanılmalı; baskın kurulan yapıya hasar verebilmeli; tüm değişiklikler aynı kayıt içinde korunmalı.

- [ ] Z1 — **Kaynak alanı:** Orman/kütük/taş/çubuk; buradan çıkan kaynaklar envanter, depo ve üretim rotalarında kullanılabiliyor.
- [ ] Z2 — **Yerleşim ve inşaat:** Temel, duvar, çatı, kapı, savunma ve karma malzeme; aynı grafik hasar, destek ve baskın tarafından kullanılıyor.
- [ ] Z3 — **Arazi cebi:** Dış yüzeyle bağlantılı izinli kazı; çukur/kısa tünel/oda, korunan bölge ve destek etkisi gösteriliyor.
- [ ] Z4 — **Çiftlik:** Yerleşime yakın tarla/hayvan alanı; ürünler ve hayvan çıktıları depoya/üretime gerçek girdi oluyor.
- [ ] Z5 — **Baskın yaklaşımı:** Dünyanın dışından gelen mantıklı bir rota; spawn, hedef, navigasyon, savunma ve yapı hasarı aynı yerleşime bağlanıyor.
- [ ] Z6 — **Atölye/lojistik/depo:** Kaynak alanı, çiftlik ve yerleşimi yollar/köprü/halat ile bağlayan gerçek bir transfer noktası.
- [ ] Z7 — **Hayatta kalma/keşif rotası:** Barınaktan açık hava alanına ve bir kalıntı/ipucuna uzanan rota; hava, ekipman, kaynak ve kalıcı keşif durumu etkili.
- [ ] Alanlar birbirinin çıktılarını gerçekten kullanıyor; sırf kontrol listesinde adları geçsin diye bağımsız dekor/test adaları oluşturulmadı.
- [ ] Demo küçük kaldı: yalnızca her davranış zincirini kanıtlamak için gereken fiziksel varlıklar, istasyonlar ve yol bağlantıları yer aldı.
- [ ] Oyuncu başlangıç noktasından ana sistemlere ulaşabiliyor; tabelalar/arayüzler eylemi açıklıyor ama mantıksal işlevin yerini almıyor.

### 13E. Entegre demo kabulü ile mekanik kabulini ayır

- [ ] Her M001–M119 için tekil kabul senaryosu/evidence satırı tamamlandı.
- [ ] Her mekanik, ilgili uçtan uca senaryoda en az bir gerçek üretici/tüketici bağlantısında kullanıldı.
- [ ] Her mekanik için uygun ön koşul ve geçersiz eylem reddi ayrıca kontrol edildi.
- [ ] Her mekanik için gereken tekrar deneme/idempotency ve kayıt/yükleme durumları kontrol edildi.
- [ ] Demo yalnızca mekaniklerin varlığını değil, sonuçların sonraki sistemler tarafından gerçekten tüketildiğini gösteriyor.
- [ ] Entegre senaryolarda bir sistemdeki değişikliklerin diğer sistemlerde beklenen etkileri kaydedildi.
- [ ] Her koşu için log/kayıt/ekran görüntüsü ve kabul sonucu saklandı; yalnızca ekran görüntüsüne dayanan gizli durum iddiaları kabul edilmedi.
- [ ] Başarısız senaryolar düzeltildi ve yeniden çalıştırıldı; çözülmeyenler teslim raporunda açıklandı.
- [ ] M001–M119 kayıt durumları yalnızca gerçek kanıta göre güncellendi.

### 13F. Son doğrulama ve kapatma

- [ ] Kaynak tutarlılık ve bağımlılık incelemesi tamamlandı.
- [ ] Gerekli UE 5.8 UHT/UBT derlemesi çalıştırıldı ve sonucu kaydedildi.
- [ ] Editor açılışı ve entegre demo seviyesinin yüklenmesi doğrulandı.
- [ ] PIE/oynanış kabul senaryoları çalıştırıldı.
- [ ] Ana uçtan uca senaryo temiz başlangıç durumundan baştan sona yeniden oynatıldı.
- [ ] Kayıt/yükleme arası kesinti senaryosu çalıştırıldı ve oyun aynı dünyada devam etti.
- [ ] Bilinen sınırlamalar, çözülmemiş hatalar ve erken erişim kapsamı not edildi.
- [ ] Dal karşılaştırması/PR tabanı ve değişen dosyalar son kez kontrol edildi.
- [ ] Kullanıcı onayıyla birleştirme/teslim kararı verildi.

## 14. Biten işler için kayıt şablonu

Her görev kapatıldığında aşağıdaki kaydı doldur. Kaydı yalnızca sohbet mesajında bırakma; bu dosyaya yaz.

### İş kaydı: [ID ve kısa başlık]
- [ ] Kabul ölçütleri tamamlandı
- [ ] Kaynak incelemesi tamamlandı
- [ ] Gerekliyse ilgili mekanik kabul senaryosu tamamlandı
- [ ] İlgili kayıt/yükleme ve yeniden deneme şartları kontrol edildi
- [ ] Kanıt dosyası/commit/PR eklendi

**Kaynak dosyaları:**  
**Commit/PR:**  
**Sonuç:**  
**Kanıt seviyesi:** Planlandı / Kaynakta mevcut-kısmi / Kaynak incelemesi tamamlandı / Motor doğrulandı / Demo kabul edildi  
**Kalan sınırlamalar:**  
**Sonraki bağımlı iş:**  

---

### İş kaydı: CORE-001 — Sabit adım rollback başarısızlığında fail-closed

- [x] Kabul ölçütleri tamamlandı (kaynak düzeyindeki kapsam için)
- [x] Kaynak incelemesi tamamlandı
- [ ] Unreal derleme/UHT tamamlandı
- [ ] Kontrollü rollback-failure/fault-injection senaryosu çalıştırıldı
- [ ] Entegre oynanış senaryosu tamamlandı

**Kaynak dosyaları:** `Source/MyProject3/WorldSimulationCoordinatorSubsystem.h`, `Source/MyProject3/WorldSimulationCoordinatorSubsystem.cpp`  
**Commit/PR:** [PR #18 — Core correctness: fail closed after fixed-step rollback failure](https://github.com/furkansarikiyak36-eng/myproject3/pull/18); `fix/world-simulation-fail-closed`.  
**Sonuç:** Daha önce rollback başarısızlığı yalnızca loglanıyordu; koordinatör sonraki sabit adımı engellemiyordu. Artık rollback başarısızlığı `bSimulationFaulted` durumunu ve bir açıklama metnini ayarlıyor. Koordinatör sonraki `AdvanceFixedStep` çağrılarını reddediyor ve `IsWorldSimulationReady` false döndürüyor. Amaç tutarsız dünyayı otomatik olarak devam ettirmek yerine güvenli şekilde durdurmak.  
**Kanıt seviyesi:** Kaynak incelemesi / statik değişiklik kontrolü; Unreal motor doğrulaması yapılmadı.  
**Kalan sınırlamalar:** Bu koruma koordinatör üzerinden ilerleyen sabit adımlar için geçerlidir. Diğer sistemler başka bir yerden bağımsız olarak ilerletilmemeli. Rollback'in tüm sistemlerde kesin atomikliği veya fiziksel aktör yan etkilerinin geri alınması henüz kanıtlanmış değildir.  
**Sonraki bağımlı iş:** CORE-002 — ownership transfer receipts ve işlem idempotency kayıtlarının snapshot/rollback/kayıt sınırları.

---

## 15. İlk yapılacak iş

Sıradaki çalışma **1A — Kaynak temeli ve PR zinciri** bölümündeki ilk boş kutudan başlar. Her iş tamamlandığında bu belge güncellenecek ve commit edilecek; sonra sıradaki bağımlı kutuya geçilecek. Bu belge, sohbet bağlamından bağımsız ilerleme kaydı olarak kullanılacaktır.
