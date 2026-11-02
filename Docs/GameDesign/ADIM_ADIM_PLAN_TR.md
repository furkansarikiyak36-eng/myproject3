# Adım adım plan (2026-11-02)

Sırayla yapılacaklar. Ayrıntı ve gerekçeler: `memory/MASTER_PLAN.md`; kararlar: `memory/DECISIONS.md`.
Her şey şimdilik greybox (basit şekiller); modeller ve animasyonlar en sonda.
"Sen oynarsın" yazan adımlar ancak sen PIE'da deneyince tamam sayılır.

## A. Dünya (landscape mantığı)

1. **Heightmap kimlik kartları** — 10 heightmap'in her biri için karakter (havza, dağlık, plato, ova, kanyon...),
   gerçek ölçek, öne çıkan özellikler, hangi bölgelerle iyi birleşir. → Sen 3–4 bölgeyi seçersin (öneri: v5 merkez).
2. **Birleşme planı** — bölgelerin yeri ve aralarındaki geçiş türü (dağ eteği → ova, plato → yamaç, havza → nehir).
3. **Eksik coğrafya** — dünyada olması gereken ama heightmap'lerde olmayan yerleri bul ve üret (havza, nehir yatağı, göl, geçit).
4. **Tek landscape** — hepsini 4×4 km tek dünyada birleştir, kenar dağlarıyla kapat.
5. **Bölgelere ayır, dikişleri düzelt** — sınırlarda basamak/dikiş kontrolü, ekran görüntüsü ve ölçüm. → Sen gezersin.
6. **Plan haritaları** — zemin türü (toprak, kil, kum, çakıl, kaya, çamur), nem, bitki örtüsü, kayalar.
7. **Su** — dere ve gölü yükseklik script'inde oy, su yüzeyini Water plugin ile dene, maliyetini ölç.
8. **Claim ve kazı** — 750×750 m claim hücreleri; claim kurulunca merkez + 4 komşu hücre kazılabilir.
   Voxel düzeltmeleri: açılış süresi, kazı takılması, mağara girişi, kazıların kaydı. → Sen kazarsın.

## B. Ortak altyapı (birçok sistemin ihtiyacı)

9. **Tek zemin sorgusu** — yükseklik, zemin türü, toprak derinliği, kazılabilir mi; bütün sistemler bunu kullanır.
10. **Hacim ve kütle** — toplu malzemeler (kil, kum, tahıl...) litre + yoğunlukla; kaplar 2/10/20/40/50/100 L.
11. **Kirlenme verisi** — su, toprak, gıda ve havada kirlilik değeri; arıtma yalnızca yapabildiğini temizler.
12. **Karakter geçişi** — birinci/üçüncü şahıs arasında geçiş, iki kamera için ortak nişan alma.
13. **Paralel kod dalı** — `game/p06-p08-blind-code-closure` içindeki yararlı kodu taşı, dalı arşivle.
14. **Simülasyon altyapısı** — uzak hayvan/bitki için sade simülasyon, 1 yıllık otomatik test.

## C. Greybox dünyada mekanikler

15. **Çekirdek döngü** — World haritasında: ağaç kes, kütük taşı, inşa et, barınak, ateş, yemek/su, üretim istasyonları,
    toplanabilir kaynaklar. → Sen oynarsın.
16. **Tarım ve hayvanlar** — tarla döngüsü, toprak; vahşi (geyik, tavşan, kurt, ayı) ve evcil (inek, koyun, tavuk)
    hayvanlar basit şekillerle. → Sen ekip biçersin, hayvan tutarsın.
17. **Zehirli dünya ve iklim** — asit yağmuru, kirli su/toprak, maske/filtre/arıtma; olay zincirleri, bölgesel iklim.
18. **Tehditler** — mantar kolonileri, 5 mutant türü (basit şekil), sinyalle gelen baskınlar, savunma yapıları.
19. **Geçiş altyapısı** — ip, köprü, yürüyüş yolu, merdiven dünyaya yerleşir. → Sen bir boşluktan geçersin.
20. **Kayıt ve performans** — her sistemin kaydı (kazılar dahil), 1 yıllık simülasyon, kare süresi ve bellek.
21. **Demo** — 4–5 saatlik akış, `MASTER_PLAN.md` bölüm 8'deki kabul listesi. → Sen baştan sona oynarsın.

## D. Görsellik (en son)

22. **Doku** — zemin türü haritasından.
23. **Modeller ve animasyonlar** — sen bulursun; greybox şekillerin yerine geçer.
24. **PCG yerleşimi** — plan haritalarını okur.

Daha sonra: ileri birleştirme (çentik, gönye, çapraz), hikâye ve tesisler, diğer insan toplulukları.
