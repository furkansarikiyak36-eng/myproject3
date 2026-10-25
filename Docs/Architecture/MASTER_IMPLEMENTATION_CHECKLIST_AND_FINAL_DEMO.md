# MyProject3 — Ana Uygulama Kontrol Listesi ve Final Demo Planı

**Proje:** MyProject3 / Unreal Engine 5.8  
**Belge rolü:** Bu belge, referans oyunlardan çıkarılan eksiklerin kapatılması için tek ana ilerleme takip noktasıdır.  
**Planın kaynak dalı:** `landscape-builder-plan`  
**Kaynak inceleme temeli:** `mechanic-detailing-pass-h-integrated-world-systems`  
**Son güncelleme:** 2026-10-26  
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
- [x] PR #16 ve PR #17'nin H dalıyla örtüşmesini kontrol et: G, F'nin bir commit ilerisi; H, G'nin 171 commit ilerisi; PR #17 H'den 16 commit ileride ve beş dokümantasyon dosyasını değiştiriyor. Kod değişikliği olan yeni çalışma ayrı dalda tutuluyor.
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
- [x] Genel Operation Ledger için kararlı snapshot/restore eklendi; sabit-adım rollback ve SaveGame v6 kapsamına alındı. Kaynak kontrolü geçti, motor testleri açık.
- [x] SaveGame, Pending operasyon varken reddediliyor; v4→v5→v6 kayıt uyumluluğu tanımlandı.
- [ ] Gerçek oynanış işlemlerini genel ledger'a seçici bağla; domain'in kendi receipt/transaction mekanizmasıyla çifte yetki oluşturma.
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
- [x] Ownership transfer receipt'leri sabit-adım rollback ve v5 SaveGame snapshot'larında korunuyor; v4 kayıtları boş receipt listesiyle güvenli şekilde taşınıyor. Statik kontrol tamamlandı, motor testleri açık.

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

- [x] `AHarvestableTreeActor` kaynak katmanı eklendi: kesme hasarı → fiziksel devrilme → düşük hız + zemin temasını belirli süre doğrulama → tek seferlik log çıktısı durum zinciri var; ağaç grounded olmadan log üretmiyor.
- [x] Ağaç çıktıları paralel envanter kaydı açmıyor; `ALogBuildPiece` aktörleri mevcut `WorldItemOwnershipSubsystem` / structural graph BeginPlay kayıt akışına giriyor ve `LogMeshVariantSetId` açıkça atanmalı.
- [ ] Ağaç türü/ölçüsü → log sayısı, uzunluk kesri ve nominal mesh seti veri tablosunu gerçek varlıklara göre tanımla.
- [ ] Ağaç FBX collision/physics mass/center-of-mass ve devrilme impulse değerlerini UE içinde ayarla; ground trace'in gerçek zemin kanallarında çalıştığını doğrula.
- [ ] Kısmi spawn hatası, yanlış/eksik mesh-set, save/load, tekrar hasat ve grounded negative-case senaryolarını Unreal kabul kapısında çalıştır.
- [ ] Ağaç boyutu/kurallarından çıkan kütük miktarı ve boyutlarını tanımla.
- [ ] Tam kütük, taşınan kütük, bırakılan kütük ve yerde serbest kütük durumlarını ayır.
- [ ] Kütüğün yuvarlanma, sürüklenme, itilme ve çarpışma davranışlarını tanımla.
- [ ] Enine kesme çıktılarının boyut ve parça kimliğini belirle.
- [ ] Boyuna yarma sonucunda iki ayrı ve doğru geometriye sahip parça oluştur.
- [ ] Yarım parçayı çeyrekleme işleminin ön koşullarını ve çıktısını tanımla.
- [ ] Yontma/kareleme işleminde kaynak durumu ile çıktı mesh'ini birlikte değiştir.
- [ ] Oluk işlemini yalnızca gerekli boyuna yarma ön koşulundan sonra kabul et.
- [x] `SplitHalf` ve `Quarter` eylemleri için kaynak-level atomik çoklu eşya sahipliği/split receipt API'si eklendi; ikinci fiziksel aktör spawn edilip graph/ownership kaydı doğrulanıyor, hata durumunda aktör ve ledger rollback'i var. SaveGame v7 ve sabit-adım rollback split receipt'lerini koruyor. UE doğrulaması açık.
- [ ] Blueprint/mesh başına `SplitHalfPieceCenterOffsetLocal` ve `QuarterPieceCenterOffsetLocal` değerlerini ayarla; bunlar sıfırsa eylem kasıtlı olarak reddediliyor.
- [ ] Kesit mesh pivot/offset/geometri/çarpışma eşleşmesini UE içinde doğrula; kabul testleri ertelendi.
- [x] Çeyrek kesit geometrisi için açık `QuarterSectionLog` türü ve `.QuarterSectionLog` yeni tanım eki eklendi; eski `.QuarterLog` işleme ekine geriye uyumluluk alias'ı bırakıldı. Bu, adlandırma/yeniden yükleme belirsizliğini daraltır; fiziksel ikinci parça üretimini tamamlamaz.
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
- [x] Nominal mesh aileleri için `FLogProcessingMeshSet` eklendi: açık SetId, base-round mesh ve `LengthFraction × CrossSectionShape → mesh` tablosu; çift ID/çift state mapping belirsizliği fail-closed reddediliyor.
- [x] Mesh-set seçimi item ownership kaydında `MeshVariantSetId` olarak saklanıyor; split çıktıları ve envanterden tekrar dünyaya çıkış aynı selector'ı taşıyor.
- [x] Kısaltılmış parçanın geometri durumu item `DefinitionId` içinde de korunuyor (ör. `LengthHalf.SplitHalfLog`); variant restore bu iki ekseni yeniden çözümlüyor; SaveGame şeması v8.
- [ ] Her nominal family için gerçek FBX'leri `ProcessingMeshSets` içine doldur; ölçülmeyen dosya adlarından ölçü varsayma.
- [ ] Hatalı/eksik table, duplicate SetId, duplicate geometry tuple, inventory round-trip, split retry ve legacy-save senaryolarını Unreal kabul kapısında çalıştır.
- [x] Legacy PieceType etiketinin yanında bağımsız LengthFraction ve CrossSectionShape metadata sözleşmesi eklendi; işlem ön koşulları kesit şekline göre kontrol ediliyor.
- [x] SM_RawLog_150_Quarter_04 gibi RawLog + Quarter adları çeyrek kesit şekli olarak çözümleniyor; standalone QuarterLog uzunluk kesriyle karıştırılmıyor.
- [ ] Gerçek FBX ölçü/pivot/yönlerini Unreal staging level'da doğrula ve her Blueprint'in mesh/offset/rotasyon ayarlarını kaydet.
- [ ] SM_RawLog_75.fbx base-round mesh eksikliğini gider veya kayıtta bilinçli fallback tanımla.
- [ ] Long_01..08 ve Short_01..04 için gerçek nominal ölçü/şekil etiketlerini belirle.
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

### İş kaydı: CORE-002 — Eşya transfer fişlerinin snapshot/rollback/kayıtta korunması

- [x] Item+receipt snapshot API eklendi; deterministik sıralama sağlandı
- [x] Geri yükleme, item ve receipt tablolarını birlikte doğrulayıp atomik olarak değiştiriyor
- [x] Sabit-adım rollback önceki receipt setini koruyor
- [x] SaveGame şeması v5'e yükseltildi ve v4 için açık uyumluluk yolu eklendi
- [x] İlgili çağrı noktaları ve değişiklikler statik kontrol edildi
- [ ] UE 5.8 UHT/UBT derlemesi
- [ ] Aynı operation ID ile tekrar denemede asıl sonuç döndüğünü gösteren runtime testi
- [ ] Rollback sonrası receipt setinin korunduğunu gösteren fault-injection testi
- [ ] v4→v5 yükleme ve v5 round-trip testi

**Kaynak dosyaları:** `WorldItemOwnershipSubsystem.h/.cpp`, `WorldSimulationCoordinatorSubsystem.cpp`, `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.cpp`  
**Commit/PR:** [PR #19 — Preserve item transfer receipts across rollback and saves](https://github.com/furkansarikiyak36-eng/myproject3/pull/19); `fix/ownership-transfer-receipt-snapshots`.  
**Sonuç:** Önceki snapshot API yalnızca item kayıtlarını içeriyordu; restore receipt haritasını sıfırlıyordu. Yeni overload item kayıtları ve transfer fişlerini birlikte dışa/içe aktarır, her iki adlandırılmış haritayı tüm veriler doğrulandıktan sonra değiştirir. Koordinatör rollback'i ve SaveGame v5 artık receipt'leri içeriyor. v4 kayıtları geçmiş fişleri içeremediğinden boş listeyle taşınır; revision guard eski transferin tekrar uygulanmasını engeller, ancak eski fişin asıl sonucu geri verilemez.  
**Kanıt seviyesi:** Statik kaynak kontrolü; Unreal derlemesi/runtime doğrulaması yapılmadı.  
**Kalan sınırlamalar:** Genel `WorldOperationLedgerSubsystem` geçmişi bu kapsamda kalıcılaştırılmadı; fiziksel aktör/terrain/structural graph kayıt eksikleri ayrı iştir.  
**Sonraki bağımlı iş:** CORE-003 — genel Operation Ledger'ın kayıt/yükleme ve işlem idempotency sınırını incelemek; ardından kalan P0 işlem/sahiplik kusurlarını kapatmak.

---

### İş kaydı: CORE-003 — Genel Operation Ledger snapshot ve SaveGame kalıcılığı

- [x] Ledger snapshot export/restore eklendi; kararlı sıralama var
- [x] Restore, canlı haritayı değiştirmeden önce tüm kayıtları doğruluyor
- [x] Sabit-adım rollback'i ledger snapshot'ını içeriyor
- [x] Pending operasyon varken SaveGame reddediliyor
- [x] SaveGame v6 operation ledger kayıtlarını yazıp geri yüklüyor
- [x] v4→v5→v6 uyumluluk yolu statik olarak kontrol edildi
- [ ] UE 5.8 UHT/UBT derlemesi
- [ ] Save/load tekrar-deneme runtime testi
- [ ] Pending-operation save-rejection runtime testi
- [ ] Rollback/fault-injection testi

**Kaynak dosyaları:** `WorldOperationLedgerSubsystem.h/.cpp`, `GameplayCoreTypes.h`, `WorldSimulationCoordinatorSubsystem.cpp`, `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.cpp`  
**Commit/PR:** [PR #20 — Persist operation ledger snapshots](https://github.com/furkansarikiyak36-eng/myproject3/pull/20); `fix/durable-operation-ledger`.  
**Sonuç:** Genel Operation Ledger artık snapshot/restore edilebilir ve tamamlanmış operation sonuçlarını SaveGame v6 kapsamında koruyabilir. Kayıt esnasında Pending operation varsa kaydetme reddedilir; ledger'da işlem günlüğü olmadığı için yarım kalmış işlemi güvenli şekilde devam ettirecekmiş gibi gösterilmez.  
**Kanıt seviyesi:** Statik kaynak kontrolü; Unreal derlemesi ve çalışma zamanı doğrulaması yapılmadı.  
**Kalan sınırlamalar:** Genel ledger henüz bütün domain operasyonlarına bağlanmış değil. Altı önemli kaynak dosyasında doğrudan çağrı olmadığını kontrol ettim; sonraki adım gerçek operasyon sahiplerini haritalandırıp uygun noktalara ledger protokolünü seçici bağlamak.  
**Sonraki bağımlı iş:** CORE-004 — domain işlem sahipliği/call-chain haritası ve ilk operasyon bağlantısı.

---

### İş kaydı: WOOD-001 — Çeyrek kesit ile çeyrek uzunluğu adlarını ayırma

- [x] Yeni `ELogPieceType::QuarterSectionLog` türü eklendi; enum sonuna eklenerek önceki değerlerin sıralaması korundu
- [x] `ELogCutAction::Quarter` artık bu açık türü ve `.QuarterSectionLog` tanım ekini üretir
- [x] `SetPieceTypeByName` açık `quartersection/quarterround` adlarını ayrı çözer
- [x] `ApplyDefinitionVariant`, yeni eki ve eski `.QuarterLog` kayıtlarını destekler
- [x] Tekrarlanan aynı işlem için eski quarter receipt uyumluluğu korundu
- [x] Statik kaynak kontrolü yapıldı; süslü parantez dengesi ve değişen çağrı noktaları kontrol edildi
- [ ] UE 5.8 UHT/UBT derlemesi
- [ ] Blueprint/content asset eşlemesi ve runtime işlem testi
- [ ] Fiziksel bölme işleminde ikinci parçanın/atık parçanın tekil aktör ve item ledger kaydıyla atomik üretilmesi
- [ ] Kaynak sahipliği, iki parçanın geometrisi/çarpışması ve save/load kabulü

**Kaynak dosyaları:** `Source/MyProject3/LogBuildingTypes.h`, `Source/MyProject3/LogBuildPiece.cpp`  
**Commit/PR:** [PR #22 — Distinguish quarter-round cross section from quarter-length log](https://github.com/furkansarikiyak36-eng/myproject3/pull/22); `fix/log-quarter-cross-section-semantics`.  
**Sonuç:** Mevcut `QuarterLog` türü ve suffix hem 1/4 uzunluk hem de çeyrek kesit şeklinde yorumlanabiliyordu. Yeni kesme eylemi artık açık `QuarterSectionLog` türü ve suffix'i kullanıyor. Eski kayıtlardaki `.QuarterLog` eki, eski işleme eyleminin uyumluluk alias'ı olarak okunuyor. Bu düzeltme durumu daha açık hâle getirir; kendi başına iki fiziksel çeyrek parça üretmez.
**Kanıt seviyesi:** Kaynak değişikliği + statik kontrol; Unreal motoru doğrulanmadı.
**Sonraki bağımlı iş:** WOOD-002 — tek kaynaktan çoklu fiziksel çıktı oluşturmak için atomik ownership split/receipt işlemini tasarlayıp uygulamak; sonra enine kesim, boyuna yarma ve parça miktarı kurallarını bağlamak.

---

### İş kaydı: WOOD-002 — Fiziksel kütük bölme için atomik çıktı sahipliği

- [x] `TrySplitItem` tek işlemde kaynak kaydı ile yeni benzersiz output item kayıtlarını üretir
- [x] Split receipt aynı OperationId ve aynı tam istek için eski output kayıtlarını döndürür; farklı istekle ID yeniden kullanımı reddedilir
- [x] Split receipt snapshot/restore kapsamına ve SaveGame şema v7'ye eklendi
- [x] Sabit-adım rollback artık split receipt'lerini de geri alır
- [x] `SplitHalf` ve `Quarter` eylemleri için deferred ikinci actor oluşturma, benzersiz kimlik bağlama ve ownership/graph kayıt doğrulaması eklendi
- [x] Kayıt veya graph doğrulaması başarısız olursa child actor, kaynak aktör transform/mesh/type ve item ledger snapshot geri alınmaya çalışılır
- [x] Kesit ofseti sıfır/uzunlamasına ise işlem önceden reddedilir
- [x] İkinci çıktının mesh-local rotasyonu split-half ve quarter-section için ayrı, Blueprint'te ayarlanabilir alanlar olarak tanımlandı ve child actor'a kopyalanıyor.
- [x] Statik yapı kontrolleri, API eşleşmeleri ve brace-balance kontrol edildi
- [ ] UE 5.8 UHT/UBT derlemesi
- [ ] Her Blueprint için split output mesh ve center-offset değerlerini konfigüre et
- [ ] PIE/runtime testi: half-round ve quarter-section iki ayrı görünür aktör ve unique item ID oluşturur
- [ ] Runtime retry testi: aynı OperationId yeni aktör üretmez, iki kayıtlı sonucu döndürür
- [ ] Failure injection: ikinci actor/graph kaydı başarısız olduğunda kaynak/ledger/graph geri alınır
- [ ] Save/load sonrası iki aktör ve split receipt/ownership ilişkisi geri kurulur

**Kaynak dosyaları:** `WorldItemOwnershipSubsystem.h/.cpp`, `LogBuildPiece.h/.cpp`, `WorldSimulationCoordinatorSubsystem.cpp`, `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.cpp`  
**Commit/PR:** [PR #23 — Atomically split logs into uniquely owned physical pieces](https://github.com/furkansarikiyak36-eng/myproject3/pull/23); `fix/atomic-log-piece-splitting`.  
**Sonuç:** Split işlemi artık yalnızca bir mesh/type değişikliği değildir. İstenen iki çıktı önce sahiplik sisteminde atomik olarak tanımlanır; kaynak ID ilk çıktıyı korur, ikinci çıktı yeni GUID alır. Child actor deferred spawn ile ledger ID'si verildikten sonra BeginPlay/graph kaydı tamamlanır. Başarısız doğrulamada rollback denenir. Center-offset ve ikinci çıktı rotasyonu mesh'e göre Blueprint'te ayarlanabilir; doğru geometri/pivot eşleşmesi yine de Blueprint ve UE runtime'da doğrulanmalıdır.
**Kanıt seviyesi:** Statik kaynak kontrolleri; Unreal motoru/PIE doğrulaması yapılmadı.
**Kalan sınırlamalar:** Genel world save physical actor transforms/graph links'leri tamamen geri kurmadığından split receipt kalıcılığı tek başına tam actor persistence sağlamaz. Bu genel persistence açığı ayrı iş olarak kalır.
**Sonraki bağımlı iş:** WOOD-003 — gerçek mesh/asset variant tablosu ve uzunluk kesri ile kesit biçimini birlikte temsil etme; ardından tam kaynak/atık miktar kuralları.

---

### İş kaydı: WOOD-003 — Uzunluk kesri ve kesit geometrisini ayırma; FBX varyant kataloğu

- [x] `ELogLengthFraction` enum'u eklendi: full, 3/4, 1/2, 1/4 length
- [x] `ELogCrossSectionShape` enum'u eklendi: full-round, half-round, quarter-round, hewn, square, groove, trough, plank, notch, joint, other
- [x] `FLogPieceGeometryState` bağımsız iki alanı taşır; `ALogBuildPiece` içinde SaveGame metadata ve Blueprint pure accessor var
- [x] Legacy `PieceType` alanı korunurken işlem eylemlerinin ön koşulları `CrossSectionShape` üzerinden kontrol ediliyor; length fraction split sırasında korunuyor
- [x] `SM_RawLog_150_Quarter_04` gibi RawLog+Quarter isimleri kesit biçimi olarak ayrılıyor; tek başına `QuarterLog` legacy length fraction olarak korunuyor
- [x] Repo envanteri doğrulandı: 48 root FBX + 10 Archive FBX; boyut grupları ve belirsiz Long/Short adları kataloğa yazıldı
- [ ] Gerçek FBX bounds/pivot/rotation UE 5.8 staging level'da doğrulandı
- [ ] `SM_RawLog_75.fbx` base-round eksikliği çözüldü
- [ ] Her blueprint output mesh, center-offset ve child rotation alanlarıyla yapılandırıldı
- [ ] UHT/UBT derlemesi ve runtime işlem testleri

**Kaynak dosyaları:** `LogBuildingTypes.h`, `LogBuildPiece.h/.cpp`, `AssetPackages/README.md`, `Docs/Architecture/WOOD_003_ASSET_VARIANT_CATALOG.md`  
**Commit/PR:** [PR #24 — Separate log length fraction from cross-section geometry](https://github.com/furkansarikiyak36-eng/myproject3/pull/24); `fix/log-geometry-dimensions`.  
**Sonuç:** LengthFraction ile CrossSectionShape artık aynı birleşik etiketten ayrı tutuluyor. Fiziksel işlem eylemleri, örneğin 1/2 uzunluğundaki bir round log üzerinde de kesit-shape ön koşullarını kullanabilir; split işlemi length fraction metadata'sını korur. FBX listesi isimlerden çıkarılmıştır, gerçek geometri ölçümü olarak sunulmuyor.  
**Kanıt seviyesi:** Kaynak değişikliği, API/koşul kontrolü ve GitHub asset-inventory incelemesi. Unreal geometri doğrulaması yapılmadı.  
**Kalan sınırlamalar:** Asset pivot/ölçü/ekseni, import edilmiş Content varlıkları ve Blueprint alan atamaları motor içinde kontrol edilmedi. `SM_RawLog_75.fbx` base mesh yok; Long/Short varlıklarının gerçek boyları belirsiz.  
**Sonraki bağımlı iş:** WOOD-005 — tree-felling/grounding ve kaynak miktarı/çıktı oluşturma zincirini açık duruma bağla. Gerçek FBX ölçümü, ProcessingMeshSets değerlerini doldurma ve runtime doğrulama ayrı açık kabul maddeleridir.

---

### İş kaydı: WOOD-004 — Data-driven nominal mesh aileleri ve sahiplikte geometri kimliği

- [x] `FLogProcessingMeshSet` ve `FLogPieceMeshVariant` eklendi; bir mesh seti tabanı ve exact `LengthFraction × CrossSectionShape → UStaticMesh` eşlemesini tutuyor
- [x] Kaynak aktörün set'i seçmesi, yalnızca explicit selector veya current mesh için tekil bir eşleşme olduğunda kabul ediliyor; duplicate SetId/base-mesh/geometry tuple belirsizse fail-closed
- [x] Kesme eligibility, action çıktısı ve definition-variant restore tek resolver'ı kullanıyor; legacy mesh property fallback yalnızca tablo boş ve selector None iken kalıyor
- [x] Split center offset/child rotasyon ayarları seçilmiş mesh setinden okunuyor; child aynı set'i miras alıyor
- [x] `MeshVariantSetId` item ownership record'una eklendi; split çıktıları ve inventory drop spawn'ı bu ID'yi koruyor
- [x] Geometry DefinitionId artık length fraction + shape birleşimini koruyabiliyor (ör. `LengthHalf.SplitHalfLog`); restore parser length fraction'ı resolver'dan önce uyguluyor
- [x] SaveGame schema v8'e çıktı; v7→v8 geçişi eski FName alanını none bırakıp dosya adından tahmin üretmiyor
- [x] Statik wiring kontrolleri ve C++ brace-balance kontrolleri geçti
- [ ] UE 5.8 UHT/UBT derlemesi
- [ ] Per-Blueprint gerçek FBX mesh setleri, center offset ve child rotations dolduruldu
- [ ] Import edilmiş FBX ölçü/pivot/local-axis doğrulaması
- [ ] Runtime: length fraction split sonrası korunuyor, iki parça farklı ID ile tek tek inventory/storage/cargo akışından geçiyor
- [ ] Runtime: SaveGame v8 round-trip ile MeshVariantSetId + geometry DefinitionId korunuyor
- [ ] Runtime: yanlış/eksik tuple, duplicate state mapping ve eksik variant asset eylemi reddediyor; kaynak state değişmiyor

**Kaynak dosyaları:** `LogBuildingTypes.h`, `LogBuildPiece.h/.cpp`, `WorldItemOwnershipSubsystem.h/.cpp`, `InventoryCraftingComponent.cpp`, `WorldSimulationSaveGame.h`, `WorldSaveCoordinatorSubsystem.cpp`  
**Commit/PR:** [PR #26 — Data-drive log mesh variants and preserve geometry identity](https://github.com/furkansarikiyak36-eng/myproject3/pull/26); `fix/log-mesh-variant-sets`.  
**Sonuç:** Tek bir nominal kaynak mesh ailesine ait işlenmiş şekiller için sabit tek mesh pointer'ları yerine açık state tuple tablosu kullanılabiliyor. Doğru set seçimi ownership record ve definition ID boyunca korunuyor. Bu, gerçek FBX'ler tabloya atanmış veya geometri UE içinde doğrulanmış demek değildir.  
**Kanıt seviyesi:** Statik kaynak incelemesi ve targeted structural checks; Unreal motor doğrulaması yapılmadı.  
**Kalan sınırlamalar:** `SM_RawLog_75.fbx` base-round mesh yok; Long/Short dosyalarının gerçek ölçüleri belirsiz. Yeni data-driven tablolar için mevcut Blueprint/content asset atamaları henüz yapılmadı.  
**Sonraki bağımlı iş:** WOOD-005 — ağaç kesme/devrilme/zemine oturma ve kaynak log miktarı/ID üretimini ownership + mesh family akışına bağla.

---

### İş kaydı: WOOD-005 — Ağaç kesme, fiziksel devrilme ve grounded hasat çıktısı

- [x] Yeni `AHarvestableTreeActor` sınıfı eklendi; ağaç hasarı, devrilme ve grounded/çıktı durumları tek aktör yaşam döngüsünde tutuluyor
- [x] Kesme hasarı pozitif ve sonlu olmalı; ağaç varlığı, log actor class ve çıktı konfigürasyonu yoksa hasar/hasat işlemi reddediliyor
- [x] Sağlık sıfırlandığında tree mesh fizik simülasyonuna geçiyor ve chop direction üzerinden impulse alıyor
- [x] Log üretimi yalnızca fizik hızı eşik altında kalıp görünürlük kanalında zemin teması `GroundedHoldSeconds` süresince doğrulanınca açılıyor
- [x] Log sayısı, `ALogBuildPiece` subclass'ı ve `LogMeshVariantSetId` açık konfigürasyon; log actor'ları var olan ownership + structural graph BeginPlay kayıt yolunu kullanıyor
- [x] Aynı tree actor'da grounded/harvest transition tekrar çalıştırılmıyor; kısmi spawn başarısızlığında otomatik tekrar yapılmıyor
- [x] Bir output actor spawn/validation aşamasında başarısız olursa bu hasat denemesindeki bütün oluşturulmuş log actor'ları gizlenip destroy edilerek kısmi görünür kaynak seti temizleniyor; actor EndPlay mevcut sahiplik lifecycle'ını kullanıyor
- [x] İlgili kaynak dosyalarında brace-balance kontrolü sıfır fark
- [ ] UE 5.8 UHT/UBT derlemesi ve Blueprint oluşturma
- [ ] Gerçek tree FBX collision, physics mass/center of mass, impulse, ground trace channel ve grounded hold süresi ayarı
- [ ] Tür/ölçü → log count/length fraction/mesh family tablosu; filename'den ölçü tahmini yapılmayacak
- [ ] Runtime: düşmeden önce log yok; grounded olmadan log yok; grounded sonrası doğru sayıda unique ownership ID
- [ ] Runtime: yanlış/eksik mesh-set, invalid log class, zemin yok, eğimli yüzey, su/boşluk, bir log spawn başarısızlığı ve tekrar hasat negative cases
- [ ] Save/load ve 119 mekanik entegre kabul senaryosu

**Kaynak dosyaları:** `HarvestableTreeActor.h/.cpp`, `LogBuildPiece.h/.cpp`, `WorldItemOwnershipSubsystem.h/.cpp`, `WorldStructuralGraphSubsystem.h/.cpp`  
**Branch:** `fix/log-mesh-variant-sets`  
**Sonuç:** İlk ağaç yaşam döngüsü adapter'ı eklendi; grounded gate hasat çıktısından önce gelir. Bu, ağaç asset'lerinin ayarlandığı, log count'un oyuna göre doğrulandığı veya motor içinde çalıştığı anlamına gelmez.
**Kanıt seviyesi:** Kaynak dosyası incelemesi ve statik yapı kontrolü. Unreal derlemesi/PIE yapılmadı.
**Kalan sınırlamalar:** Actor tree mesh asset'ini Blueprint'ten alır; gerçek collision/pivot/physics ayarları henüz bilinmiyor. Hasat çıktısı nominal mesh family selector ister. Actor transform/lifecycle SaveGame restore ile henüz entegre değil. Kısmi spawn actor'larını temizleme kaynak katmanı eklendi; gerçek actor/ledger davranışı, motor doğrulaması, otomatik retry ve tam işlem rollback'i kanıtlanmadı.
**Sonraki bağımlı iş:** WOOD-006 — mevcut `LogCarryComponent` ve actor lifecycle sınırları denetlendi; serbest kütüğe sınırlı itme/yuvarlanma kaynak API'si eklendi. Kaynak miktarı/production stoğu aktarımı ve transform kalıcılığı WOOD-007 kapsamında açık kalıyor.

---

### İş kaydı: WOOD-006 — Serbest kütük için doğrulanmış itme/yuvarlanma eylemi

- [x] Mevcut taşıma akışı denetlendi: pickup, held-owner transferi, snap/free placement ve drop mevcut ownership/lifecycle kapılarını kullanıyor.
- [x] `ULogCarryComponent::TryPushOrRollAimedLog` eklendi; yalnızca hedeflenen, serbest, elde olmayan, yapıya bağlı olmayan ve `World` sahipliğindeki tekil fiziksel kütük kabul ediliyor.
- [x] Kimlik/ownership kaydı, Quantity=1, aktif rigid-body simülasyonu ve çarpışma ön koşulları kontrol ediliyor; envanter/kargo/yığın kaydı fiziksel hedef olarak kabul edilmiyor.
- [x] Sonlu ve ayarlanabilir impulse gücü sınırlandırılıyor; impulse hit noktasına uygulanarak doğal dönme momentine izin veriliyor.
- [x] İsteğe bağlı doğrudan input için V bağlaması ve Blueprint/Enhanced Input üzerinden çağrılabilir API eklendi.
- [x] Kaynak düzeyinde çağrı noktası, koruma koşulları, brace dengesi ve ilgili semboller statik kontrol edildi.
- [ ] UE 5.8 UHT/UBT, Editor ve PIE doğrulaması.
- [ ] Gerçek mesh kütlesi, sürtünme, çarpışma profili ve impulse değerlerini sahnede ayarlama.
- [ ] Fizik hareketi durduğunda authoritative item WorldLocation bilgisini güvenli biçimde eşitleme; SaveGame fiziksel transform geri yüklemesiyle bağlama.
- [ ] Yerden sürükleme için ayrı, açık bir oyuncu etkileşimi tasarlama; mevcut held carry bunu tek başına tamamlamaz.
- [ ] Fiziksel parça sayısı/miktarı ile `WorldProductionSubsystem` stok ve tarif girdileri arasındaki gerçek aktarımı kurma.
- [ ] İtme sonrası sahiplik, pickup tekrar denemesi, destek grafiği ve save/load negatif durumlarını Unreal içinde kabul etme.

**Kaynak dosyaları:** `Source/MyProject3/LogCarryComponent.h/.cpp`, `LogBuildPiece.h/.cpp`, `WorldItemOwnershipSubsystem.h/.cpp`  
**Branch/PR:** `fix/log-mesh-variant-sets`; [PR #26](https://github.com/furkansarikiyak36-eng/myproject3/pull/26)  
**Sonuç:** Kütüğü elde taşıma/yerleştirme akışına dokunmadan, serbest fiziksel kütük için açık bir itme/yuvarlanma eylemi eklendi. Bu, yerden sürükleme, fiziksel transform kalıcılığı veya üretim stok entegrasyonunun tamamlandığı anlamına gelmez.  
**Kanıt seviyesi:** Kaynak değişikliği + statik kontrol; Unreal motoru doğrulanmadı.  
**Sonraki bağımlı iş:** WOOD-007 — fiziksel kütük/tahta çıktılarını tekil sahiplikten üretim stokları, tarif girdileri ve çıktı aktörlerine kayıpsız bağlama.

---

### İş kaydı: WOOD-007 — Fiziksel kütüğü üretim istasyonu stokuna güvenli alma

- [x] FWorldProductionPhysicalIntakeReceipt eklendi; işlem ID'si, kaynak item ID/revizyonu, istasyon revizyonu, kaynak DefinitionId, mesh-family ID, stok anahtarı ve sonuç revizyonlarını kaydediyor.
- [x] TryIngestWorldOwnedLogPiece eklendi: yalnızca kimliği geçerli, tekil (Quantity == 1), World sahipliğinde, serbest, elde/yerleşimde/bağlı olmayan fiziksel ALogBuildPiece kabul ediliyor.
- [x] Üretim istasyonunun mevcut miktarı, kapasitesi ve devam eden işlerin rezerve çıktı kapasitesi kaynak sahipliğini değiştirmeden önce denetleniyor.
- [x] Kabul edilen fiziksel item sahiplik defterinde World → Transformed geçişi yapıyor; istasyon stok miktarı ve intake fişi aynı kaynak çağrısında güncelleniyor, fiziksel aktör gizlenip devre dışı bırakılıyor ve destroy ediliyor.
- [x] Stok anahtarı DefinitionId ile varsa MeshVariantSetId bileşimini koruyor; nominal mesh aileleri sessizce tek bir genel stok türüne katılmıyor.
- [x] Aynı OperationId ve aynı istek için retry mevcut fişi döndürüyor; aynı kaynak ID'sinin farklı işlem/istasyonla ikinci kez alınması reddediliyor.
- [x] Intake fişleri istasyon snapshot'ının parçası; SaveGame şeması v9'a yükseltildi ve v8→v9 geçişi eski kayıtlarda boş yeni fiş dizisiyle uyumlu.
- [x] Restore sırasında intake operation/source ID tekrarları ve geçersiz revizyonlar fail-closed reddediliyor.
- [x] Kaynak dosyalarında brace balance ve declaration/definition/migration wiring statik kontrol edildi.
- [ ] UE 5.8 UHT/UBT ve runtime retry/failure-injection testleri.
- [ ] Gerçek üretim tariflerinin MakePhysicalStockItemId anahtar sözleşmesine göre yapılandırılması.
- [ ] Tarif girdisinin stoktan atomik tüketilmesi, işin tamamlanması ve çıktı item/aktörlerinin aynı sahiplik akışında üretilmesi.
- [ ] İstasyon stoklarına girmiş kaynakların save/load sonrası tarifler ve intake receipt'leri ile çapraz doğrulanması.
- [ ] Stok girişinde actor destroy başarısızlığı ve save sırasında mevcut fiziksel aktörlerin tutarsız kalmaması için Unreal kabul testi.

**Kaynak dosyaları:** WorldProductionSubsystem.h/.cpp, WorldSimulationSaveGame.h, WorldSaveCoordinatorSubsystem.cpp, WorldItemOwnershipSubsystem.h/.cpp  
**Branch/PR:** fix/log-mesh-variant-sets; [PR #26](https://github.com/furkansarikiyak36-eng/myproject3/pull/26)  
**Sonuç:** Fiziksel ve serbest dünya kütüğünü üretim istasyonundaki miktar tabanlı stoğa alma için kaynak katmanı eklendi. Kaynak sahipliği değiştirilmeden önce kapasite doğrulanıyor; değişiklik ve kalıcı intake fişi birlikte commit ediliyor. Bu iş henüz fiziksel makine/animasyon, tarif tüketimi veya çıktı aktörü oluşturma döngüsünü tamamlamıyor.
**Kanıt seviyesi:** Kaynak değişikliği + statik yapı kontrolü. Unreal motoru doğrulanmadı.
**Kalan sınırlamalar:** Stok yalnızca aggregate miktarı temsil eder; Transformed kaynak kaydı ve intake receipt fiziksel aktörü tanımlar, ancak tamamlanmış üretim işinin recipe tüketimi/output spawn/save-load çapraz bütünlüğü ayrı WOOD-008 işidir.
**Sonraki bağımlı iş:** WOOD-008 — tarif başlatma/tamamlama, stock consumption, output capacity reservation ve output item creation arasında tek bir tekrar-denemeye dayanıklı üretim işlemi kurmak.

---

### İş kaydı: WOOD-008 — Üretim işi kuyruğa alma/toplama için kalıcı retry fişleri

- [x] FWorldProductionEnqueueReceipt ile OperationId → RecipeId/JobId/revizyon sonucu saklanıyor.
- [x] TryEnqueueJob açık OperationId ve ExpectedStationRevision ister; aynı tam istek retry edildiğinde aynı JobId döner ve kaynaklar ikinci kez düşülmez.
- [x] Üretim girdileri, iş kuyruğu limiti, istasyon etiketi ve rezerve edilmiş/gelecek çıktı kapasitesi değişiklikten önce doğrulanıyor; stok ve iş kaydı ancak tüm ön kontroller geçince güncelleniyor.
- [x] FWorldProductionCollectReceipt ile tamamlanmış işin çıkarılması kalıcı retry fişine bağlandı; aynı OperationId retry'si ikinci kez job removal yapmadan başarı döner.
- [x] İşe alma, fiziksel stok kabulü ve job collection fişleri arasında aynı production OperationId'nin türler arası tekrar kullanımı reddediliyor.
- [x] Receipt dizileri station snapshot'ında saklanıyor; SaveGame schema v10 ve v9→v10 migration eklendi.
- [x] RestoreSnapshot receipt kimlik/revizyonlarını, recipe referanslarını, duplicate operation ID'leri ve collected job tutarlılığını kontrol ediyor.
- [x] İlgili C++ dosyalarında declaration/definition, migration ve brace-balance statik kontrol edildi.
- [ ] UE 5.8 UHT/UBT ve enqueue/collect aynı-ID retry/failure-injection runtime testleri.
- [ ] Tarife girdi tüketimi ile tamamlanma çıktısının item ownership kaydına aktarılması hâlâ ayrı işlem zinciri.
- [ ] Tarif çıktısının fiziksel aktör olarak üretilmesi veya depoya/enventere aktarılması; üretim makinesi sunumu ve ilgili save/load testleri açık.

**Kaynak dosyaları:** WorldProductionSubsystem.h/.cpp, WorldSimulationSaveGame.h, WorldSaveCoordinatorSubsystem.cpp  
**Branch/PR:** fix/log-mesh-variant-sets; [PR #26](https://github.com/furkansarikiyak36-eng/myproject3/pull/26)  
**Sonuç:** Üretim işini kuyruklama ve tamamlanmış iş kaydını toplama için açık, kalıcı OperationId API'leri eklendi. Geriye dönük EnqueueJob/CollectCompletedJob sarmalayıcıları korunuyor; retry garantisi isteyen yeni çağıranlar TryEnqueueJob/TryCollectCompletedJob kullanmalı.
**Kanıt seviyesi:** Kaynak değişikliği ve statik wiring/brace kontrolü; Unreal motoru doğrulanmadı.
**Kalan sınırlamalar:** EnqueueJob/CollectCompletedJob eski sarmalayıcıları her çağrıda yeni ID ürettiğinden çağıranın tekrar isteğini ayırt edemez. Tarif tamamlanması şu anda aggregate stock ekliyor; bütün giriş/çıkışın fiziksel item/aktör ve ownership ile atomik eşleşmesi tamamlanmadı.
**Sonraki bağımlı iş:** WOOD-009 — üretim sonucunun gerçek item kaydı/physical actor/stock/depo ile tutarlı oluşturulması ve station output materialization işleminin retry-safe yapılması.

---

### İş kaydı: WOOD-009 — Üretim stoğundan fiziksel kütük oluşturma

- [x] TryMaterializeStockAsWorldLog eklendi; çağrı stok anahtarı, DefinitionId, BaseDefinitionId ve MeshVariantSetId'nin aynı sözleşmeye uymasını doğruluyor.
- [x] İstasyon revizyonu ve eldeki stok miktarı spawn öncesi ve Blueprint/BeginPlay akışından sonra yeniden doğrulanıyor.
- [x] Yeni output actor deferred spawn ile hidden/inert başlatılıyor; benzersiz item ID, definition/family ve graph node BeginPlay sonrası kontrol ediliyor.
- [x] Varyant DefinitionId gerçek mesh ailesine ApplyDefinitionVariant üzerinden bağlanıyor; eksik/yanlış mesh tuple'ı varsa stok azaltılmadan actor geri alınıyor.
- [x] Başarılı materialization fiziksel item için tekil World sahiplik kaydı oluşturuyor, stoktan bir birim indiriyor ve WorldItemId içeren kalıcı fiş kaydediyor.
- [x] Aynı OperationId ve aynı istek tekrarlandığında yeni actor/ikinci stok azaltımı yapılmıyor; farklı istekle aynı ID reddediliyor.
- [x] Materialization fişleri station snapshot'ına dahil; SaveGame schema v11 ve v10→v11 migration eklendi.
- [x] Save/load öncesi intake ve materialization fişlerinin item ownership defterinde karşılığı olması denetleniyor.
- [x] Operation ID'lerinin intake/enqueue/collect/materialize türleri arasında çakışması reddediliyor; restore duplicate output IDs ve stock-key tutarsızlığını fail-closed ele alıyor.
- [x] İlgili dosyalarda source wiring, schema migration, brace-balance kontrolleri yapıldı.
- [ ] UE 5.8 UHT/UBT, Editor/PIE, gerçek mesh family, physics/collision ve failed-spawn rollback kabul testleri.
- [ ] Fiziksel aktör dönüşümleri genel SaveGame manifestine alınmalı; receipt yalnızca kimliği saklıyor, konum/dönüş/physics state'i geri kurmuyor.
- [ ] Çıktıyı doğrudan depo/envantere alma ve fiziksel olmayan ürün türlerini aynı sahiplik sözleşmesine bağlama.
- [ ] Stok anahtarlarını gerçek tarif/Blueprint verisine atama; üretim işinin recipe outputs ile materialization arasında uçtan uca çalıştığını doğrulama.

**Kaynak dosyaları:** WorldProductionSubsystem.h/.cpp, WorldSimulationSaveGame.h, WorldSaveCoordinatorSubsystem.cpp, WorldItemOwnershipSubsystem.h/.cpp, LogBuildPiece.h/.cpp  
**Branch/PR:** fix/log-mesh-variant-sets; [PR #26](https://github.com/furkansarikiyak36-eng/myproject3/pull/26)  
**Sonuç:** Aggregate üretim stokundan, birim miktarı düşürerek benzersiz kimlikli fiziksel kütük aktörü oluşturma kaynak katmanı eklendi. Stok azalması ancak aktörün kimlik, mesh-varyant ve yapısal graph doğrulamaları tamamlandıktan sonra commit ediliyor.  
**Kanıt seviyesi:** Kaynak değişikliği + statik yapısal kontrol; Unreal motoru doğrulanmadı.
**Kalan sınırlamalar:** Fiziksel aktör transform/lifecycle save/load hâlâ genel kayıtta yer almıyor. Recipe output oluşturulması stok seviyesine kadar çalışıyor, ancak gerçek tarif verisi, makine aktörü ve save/load sonrası fiziksel dünyayı tam geri kurma kapısı açık.
**Sonraki bağımlı iş:** WOOD-010 — fiziksel aktör transformları, mesh/physics state, structural graph bağlantıları ve item ownership'i tek bir SaveGame/restore aşamasında yeniden kurmak; ayrıca mevcut drop/intake/materialize akışında actor-state tutarlılığını sağlamlaştırmak.

---

## 15. İlk yapılacak iş

WOOD-004 data-driven mesh-set selector, geometry tuple resolver, ownership selector ve SaveGame v8 kaynak katmanı mevcut; gerçek FBX/Blueprint ataması ve UE doğrulaması açık. WOOD-005 ağaç devrilme/grounded adapter'ı mevcut; gerçek fizik/asset ve boyut→çıktı tablosu açık. WOOD-006 serbest kütüğe sınırlı push/roll kaynağı eklendi; drag, world-location kalıcılığı ve production-stock entegrasyonu tamamlanmadı.

WOOD-007 fiziksel kütük → station stock girişini, WOOD-008 iş kuyruğu/toplama retry fişlerini ve WOOD-009 stock → fiziksel log materialization kaynak katmanını ekledi; SaveGame şeması v11. Sıradaki iş **WOOD-010 — fiziksel aktör transform/lifecycle save-load**. Gerçek FBX/Blueprint atamaları, üretim tariflerinin anahtarlarla eşlenmesi ve UE kabul testleri hâlâ açık. Sonrasında ahşap durum/mesh açıkları, taş/çubuk ve ortak yapı sistemine devam et. 75/150/300/600 veya Long/Short dosya adlarından ölçü tahmin etme. Diğer referans oyunlar ve LB0–LB4 sırayla ele alınacak; final demo en sonda.
