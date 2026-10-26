# VerticalSlice_C — Dere Kenarı Açıklığı

Harita: `Content/DemoAssets/Maps/VerticalSlice_C`
Dünyayı kuran sınıf: `AVerticalSliceCWorldActor` ([VerticalSliceCWorldActor.cpp](../../Source/MyProject3/VerticalSliceCWorldActor.cpp))
Haritayı yeniden oluşturma: [CreateVerticalSliceCMap.py](../../UnrealEditorScripts/CreateVerticalSliceCMap.py)

![Açılış](VerticalSlice_C/01_start.jpg)

## Bu sahne nedir

Yaklaşık 70 × 70 metrelik, tek parça bir orman açıklığı. İstasyon, tabela ya da test köşesi yok. Mekanikleri yerin kendisi istiyor: terk edilmiş bir kamp, kapısı ve çatısı olmayan bir kulübe, yürüyerek geçilemeyen bir dere ve onun yıkık köprüsü, merdiveni çürümüş bir kayalık, tepedeki bir gözcü direği ile zipline'ı ve doğuda yıkık bir kazık duvar.

Oyuncu sönmüş ateşin yanında doğar ve yarım kalmış kulübeye bakar. Ekranda tek bir not çıkar: kamp terk edilmiş, kulübenin kapısı ve çatısı yok, köprü yıkık, geceler soğuk. Gerisini oyuncu kendisi keşfeder.

![Genel görünüm](VerticalSlice_C/02_overview.jpg)

Yukarıdan bakış: önde dere ve kıyı kayaları, ortada kamp ve sundurma, sağda kulübe, solda kazık duvar, arkada yamaçtaki kayalık basamak ve gözcü direği. Haritanın çevresinde tepeler ve uzak bir orman var; dünyanın kenarı görünmez.

## Bölgeler ve neler yapılabilir

### Kamp

![Kamp](VerticalSlice_C/04_camp.jpg)

| Ne var | Ne yapılır | Mekanik |
|---|---|---|
| Taş halkalı, sönmek üzere bir ateş | Küçük odun (en fazla 1/4 boy) taşıyıp ateşe E ile at; yanma süresi ağırlığa göre artar | Yakıt, ısı kaynağı |
| Ateşin yanında yarılmış küçük parçalar | Al, at, depola | Yakacak odun |
| Çatı direklerinden biri düşmüş bir sundurma | Düşen direği yerine koy, Delete ile gereksiz bir desteği sök | Yapı grafiği, çoklu destek, söküm |
| İçinde 3 kütük olan küçük kütük deposu | Boş elle E bir kütük çıkarır; yakına bırakılan kütükler kendiliğinden içine girer | Depo, otomatik toplama |
| Yakacak odun deposu | Sadece 1/4 boy ve daha küçük parçaları kabul eder | Depo filtresi |
| Kızak | T ile çek, üstüne kütük yükle; yük arttıkça yavaşlarsın | Kızak, taşıma yükü |
| Yerde kesilmiş kütükler | Al, taşı, kes, fırlat, yerleştir | Taşıma, kesim, fırlatma |

### Kulübe

![Kulübe](VerticalSlice_C/03_cabin.jpg)

Yedi sıra çentikli kütük duvar var, ama kapı, pencere ve çatı yok. Yanında yarılmış kalaslar ve uzun kütükler yığılı.

- **Kapı:** ön duvara bak ve K'ye bas. Duvar kütükleri açıklığın iki yanından kesilir, ortadaki parça önüne düşer, en üst sıra lento olarak kalır. Açıklığa 4 yarılmış kalas taşıyıp E ile tek tek koy. Kapının menteşe tarafı ilk kalasın konduğu yandır, kapı dışa açılır. E açar ve kapatır; L yalnızca içeriden kilitler.
- **Pencere:** P. Açıklık bir dikmeyi keseceğinde işlem reddedilir.
- **Çatı:** sağ tıkla Döşeme moduna geç, uzun bir kütüğe nişan al. Kütük iki üst duvar kütüğünün arasına oturur, fazla gelen boy kesilip ayağına düşer. Yan yana dizilen kütükler çatıyı oluşturur ve sol üstteki "Shelter" değeri yükselir.
- **Duvarı uzatmak:** Yatay modda bir duvar kütüğüne nişan al. Kütük 75 cm'lik ızgaraya oturur: kütüğe paralel tutarsan bir sonraki sıra olarak uzunlamasına biner, R ile çevirirsen köşe gibi çaprazlama oturur. Dikey modda kütüğün üstüne direk dikilir.

### Dere ve eski köprü

![Dere ve köprü](VerticalSlice_C/05_creek_bridge.jpg)

Dere yaklaşık 2,4 m derin ve kıyıları tırmanılamayacak kadar dik. Düşen biri ancak en doğudaki sığ geçitten (basamak taşları) çıkabilir.

- İki kıyıda eski köprünün direkleri duruyor, aralarında tek bir halat sağlam kalmış.
- **Halat köprü:** H ile ikinci halatı karşı kıyıdaki direklere bağla. Döşeme modunda yanındaki kalasları iki halatın arasına dizersen köprü olur.
- **Kütük köprü:** kıyıda 6 m'lik bir kütük yatıyor. Onu dereye dik olarak iki kıyıya oturtmak en hızlı geçiş.
- Kıyıdaki taşlar ve yerdeki çubuklar da alınıp yerleştirilebilir.

### Yamaç: kayalık basamak

![Kayalık ve merdiven kirişleri](VerticalSlice_C/06_cliff_stairs.jpg)

Yamacın ortasında 2,5 m'lik dik bir kayalık var. Basamakları çürümüş eski bir merdivenin iki kirişi kayalığa dayalı duruyor; dibinde kısa kalaslar var.

- **Merdiven:** sağ tıkla Merdiven moduna geç, kirişlere nişan al. Her kalas kirişlerin arasına bir basamak olarak oturur, aralık 40 cm. Döşeme modunda ise kalaslar bitişik dizilir ve rampa olur.
- Merdiven yapmak istemeyen, yamacın batısından dolanan uzun yolu kullanabilir.
- Patikadaki yaşlı, büyük ağaç bir platform için uygun: ağaca dayanan kütükler gövdeyi destek olarak kullanır. Ağaç kesilirse platform düşer.

### Gözcü noktası ve zipline

![Gözcü noktası](VerticalSlice_C/07_lookout_zipline.jpg)

Tepede uzun bir direk var. Direkten kampın deposunun yanına uzanan eski bir zipline ve yanında kesilmiş kütükler duruyor.

- Bir kütük taşıyıp direğin dibinde zipline'a bak ve E'ye bas. Kütük halat boyunca kayar ve aşağıda yere iner. Depoya yakın düştüğü için kendiliğinden depoya girer.
- Tepede Z ile bir plan seç (örneğin Gözcü Kulesi) ve Y ile yere koy. Planın hayalet kütüklerine kütük taşıyarak yapıyı kurarsın.

### Doğu: yıkık kazık duvar

![Kazık duvar](VerticalSlice_C/08_stake_wall.jpg)

Ormana bakan eski bir kazık duvar. Bazı kazıklar sivri, bazıları devrilmiş, kapının olduğu yerde bir boşluk kalmış. Duvarın ötesindeki ormanda biri dolaşıyor ve oyuncuya yaklaşıyor.

- **Sivriltme:** dik duran bir kütüğe X. Uç, parçanın kendi kesitine göre şekillenir: yuvarlak kütükte yuvarlak, çeyrekte kama, kalasta yassı olur. Sivri uca tırmanmaya çalışan yaralanır ve geri itilir.
- **Onarım:** devrilen kazıkları Dikey modda yerine dik.
- **Geçit:** bir kazığa K. Kazıklar sökülür ve geçit açılır; geçide kalaslarla kapı yapılır.
- **Fırlatma:** Q ile kütük, taş ya da çubuk fırlat. Hızlı bir çarpma tavşanı avlar, çalıyı ezer, düşmanı sendeletir ve yaralar.

### Gece

![Gece](VerticalSlice_C/09_night.jpg)

Güneş gün boyunca hareket eder; oyun 13:30'da başlar. Gece hava kararır, kar yağar ve soğuk artar. Sol üstteki "Shelter" değeri, oyuncunun üstündeki çatıya ve etrafındaki duvarlara göre hesaplanır ve hayatta kalma sistemine gider. Bu yüzden kulübenin çatısı gerçek bir ihtiyaçtır.

## Tüm mekanikler

| Grup | Mekanik | Nasıl |
|---|---|---|
| Kütükle etkileşim | Alma, taşıma, bırakma | E / G |
| | Fırlatma ve çarpma hasarı (hayvan, düşman, bitki, yapı, oyuncu) | Q |
| | İtme, yuvarlama, sürükleme | V / B basılı tut |
| | Yatay, dikey ve 45° yerleştirme; yokuşta yatay kütük eğime uyar | Sağ tık ile mod |
| | 75 cm ızgarayla dizme: uzunlamasına, çaprazlama, direk | Yatay/Dikey modda kütüğe nişan |
| | Uç snap'i (kütük ucu kütüğe), uç kısmı diğerinin içine gömülür | Çapraz mod ve dikme uçları |
| | Bir ucu bağlı kütüğün boş ucu menteşe gibi aşağı iner; altına parça konunca kalkar | Otomatik |
| Kesim | Yarma (yarım, çeyrek), yontma (1 ve 2 yüz), kalas, kiriş, oluk, tekne | C seç, F uygula |
| | Boy kesimi: 1/2, 1/4, 1/8 (her kesitte) | C seç, F uygula |
| | Kesik çizgili önizleme; yeşil = olur, kırmızı = olmaz | Nişan al |
| | Yerleştirirken fazla boyun otomatik kesilmesi | Döşeme modu ve planlar |
| Taş ve çubuk | Alma, üst üste koyma, kütüğe ya da taşa bağlama, yere dikme | E |
| | Çubuk açısı: dik, 45°, yatay | Sağ tık ile mod |
| Yapı | Duvar, çerçeve, köşe | Dizme ve snap |
| | Zemin, tavan, çatı, köprü güvertesi | Döşeme modu |
| | Merdiven ve rampa | Merdiven / Döşeme modu |
| | Payanda: bir parçanın ucu diğerinin altına değince onu taşır | Otomatik |
| | Geçici desteği sökme (yük diğer desteğe geçer) | Delete |
| | Kapı, pencere, geçit; menteşe, açılma yönü, içeriden kilit | K / P / L |
| | Sivri kazık ve savunma duvarı | X |
| | Ağaca dayalı platform (ağaç kesilince düşer) | Ağaca dayayarak yerleştir |
| | Hazır planlar: duvar, sundurma, kule, küçük ve büyük depo, kızak, kazık duvar | Z seç, Y koy |
| Lojistik | Küçük (8), büyük (24) ve yakacak odun (32) deposu, otomatik toplama | E |
| | Kızak: temel 8 kütük, gelişmiş 16 parça | T |
| | Halat köprü ve zipline | H iki kez |
| Hayatta kalma | Ateşe yakıt, barınak değeri, gün-gece, gece soğuğu | Otomatik |
| Dünya | Ağaç kesme (kesilen ağaç kütük bırakır) | Sol tık |

F1 tüm tuşları oyunun içinde gösterir.

## Bilinen sınırlar

- **Görseller:** ağaçlar, kayalar, çimenler, kapı tahtaları, sivri uçlar ve canlılar Unreal'ın temel şekilleri. Gerçek modeller için bir varlık paketi (örneğin Fab'den bir orman paketi) gerekiyor. Kütükler gerçek modeller.
- **Kısaltılmış kütükler:** kesme ve otomatik kısaltma mesh'i boyuna esneterek yapılıyor. Esnetilen parçalarda kabuk dokusu biraz uzar.
- **Kayıt:** sivri uç, kapı, halat ve planlar oyun kaydına girmiyor.
- **Malzeme bedeli:** halat ve balta istenmiyor.
- **3/4 boy kesimi kapalı:** 450 cm'lik mesh yok.
- **Elle deneme:** mekaniklerin mantığı 30 otomatik testle doğrulandı. Bu sahne de testle kuruluyor: ağaç sayısı, derenin derinliği, sığ geçit, kayalık, halatlar. Ama oynanış elle tam bir tur oynanarak denenmedi.

## Görüntüleri yeniden almak

Oyun komut satırıyla açılıp kamera bir noktaya konur ve tek kare çekilir:

```
UnrealEditor.exe MyProject3.uproject /Game/DemoAssets/Maps/VerticalSlice_C -game -NoStartView
    -ExecCmds="EnableCheats, ghost, BugItGo X Y Z Pitch Yaw 0, HighResShot 1920x1080"
```

`-NoStartView` açılıştaki kamera yönünü kapatır, `-StartHour=21.5` gece başlatır. Bu sayfadaki görüntüler bu yolla alındı.
