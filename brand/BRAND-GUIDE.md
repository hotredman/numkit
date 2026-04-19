# numkit — brand guide

Финальное руководство по фирменному стилю проекта.

---

## Концепция

**numkit** — считай и исследуй, где угодно.

Домены:
- **numkit.dev** — основной
- **numkit.io** — дублёр, 301-редирект → `.dev`

### Знак

`[nₖ]` — три прочтения в одном символе:

1. **Имя проекта** — `n` и `k` это первая и последняя буквы `numkit`
2. **Математическая нотация** — `nₖ` это элемент последовательности по индексу
3. **Синтаксис массива** — `[ … ]` это квадратные скобки как оператор массива

Знак одновременно инициализирует бренд, цитирует математику и ссылается
на синтаксис числовых языков.

---

## Типографика

### Шрифты

| Назначение | Шрифт | Вес | Файл |
|---|---|---|---|
| **Знак** `[nₖ]` | Fraunces | SemiBold 600 | `Fraunces-SemiBold.ttf` |
| **Wordmark** primary | Outfit | SemiBold 600 | `Outfit-SemiBold.ttf` |
| **Taglines** и вспомогательный текст (EN) | Geist | Medium 500 | `Geist-Medium.ttf` |
| **Wordmark** и заголовки (RU) | Inter | Medium 500 | `Inter-Medium.ttf` |
| **Основной текст** | Inter | Regular 400 | `Inter-Regular.ttf` |
| **Код** и моно | JetBrains Mono | Regular/Medium | `JetBrainsMono-*.ttf` |

### Про шрифты wordmark

**Outfit SemiBold** — основной шрифт wordmark. Современный геометрический гротеск
с характерными деталями (округлая u, изящная k) — хорошо пересекается по
характеру с засечками Fraunces в знаке. Покрывает только латиницу.

**Geist Medium** — для английских таглайнов и вспомогательных подписей
(содержит пунктуацию в латинице).

**Inter Medium** — для русских таглайнов (имеет полную кириллицу + пунктуацию).
Используется везде, где нужна кириллица.

### Fallback-стек

```css
font-family: 'Outfit', 'Inter', system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif;
```

---

## Цвета

| Токен | Светлая тема | Тёмная тема | Применение |
|---|---|---|---|
| **brand** | `#6366f1` | `#818cf8` | Знак, акцентные элементы |
| **text** | `#0f172a` | `#f1f5f9` | Wordmark, основной текст |
| **muted** | `#64748b` | `#94a3b8` | Подзаголовки, мета |
| **surface** | `#ffffff` | `#0f172a` | Фон |
| **surface-alt** | `#f8fafc` | `#1e293b` | Подложки карточек |
| **border** | `#e2e8f0` | `#334155` | Разделители, рамки |

### CSS-переменные

```css
:root {
  --brand: #6366f1;
  --brand-dark: #818cf8;
  --text: #0f172a;
  --muted: #64748b;
  --surface: #ffffff;
  --surface-alt: #f8fafc;
  --border: #e2e8f0;
}

@media (prefers-color-scheme: dark) {
  :root {
    --brand: #818cf8;
    --text: #f1f5f9;
    --muted: #94a3b8;
    --surface: #0f172a;
    --surface-alt: #1e293b;
    --border: #334155;
  }
}
```

### Правила применения

- **Знак** всегда индиго. На цветном фоне — белый.
- **Wordmark** всегда графит на светлом / почти-белый на тёмном. Не окрашивай wordmark в брендовый цвет — знак уже даёт акцент.
- На индиго-фоне всё белое.

---

## Пропорции знака и wordmark

Ключевой принцип: **знак визуально доминирует над wordmark**.

- Высота скобок `[ ]` ≈ `высота букв numkit × 1.2`
- Горизонтальный отступ между знаком и wordmark ≈ `высота знака × 0.08`
- Знак и wordmark стоят на одной базовой линии
- Субскрипт `k` опускается на `0.22 × главный размер`

---

## Файлы

### Логотипы

| Файл | Размер | Применение |
|---|---|---|
| `numkit-logo-light.svg` | 416×160 | Светлый фон. Основной логотип для сайта, README, документации. |
| `numkit-logo-dark.svg` | 416×160 | Тёмный фон. |
| `png/logo-light.png` | 520×200 | Растр @1x. |
| `png/logo-light-2x.png` | 1040×400 | Retina @2x. |
| `png/logo-dark.png` | 520×200 | Растр @1x, тёмный. |
| `png/logo-dark-2x.png` | 1040×400 | Retina, тёмный. |

### Знак

| Файл | Размер | Применение |
|---|---|---|
| `numkit-mark-light.svg` | 160×160 | Аватар GitHub, соцсети, квадратные контексты. |
| `numkit-mark-dark.svg` | 160×160 | То же, тёмный. |

### Favicon

| Файл | Размер | Применение |
|---|---|---|
| `numkit-favicon.svg` | 128×128 | Основной, без субскрипта `k` (на малых размерах не читается). |
| `numkit-favicon-dark.svg` | 128×128 | Для браузеров с выбором по теме. |
| `png/favicon-256.png` | 256×256 | Apple touch icon. |
| `png/favicon-64.png` | 64×64 | Крупный. |
| `png/favicon-32.png` | 32×32 | Стандартный. |
| `png/favicon-16.png` | 16×16 | Для старых браузеров. |
| `png/favicon.ico` | multi-size | Мульти-размер для Windows. |

### App icon (PWA/mobile)

| Файл | Размер | Применение |
|---|---|---|
| `numkit-app-icon.svg` | 512×512 | Знак на индиго-фоне со скруглением 22%. |
| `png/app-icon-1024.png` | 1024×1024 | iOS App Store. |
| `png/app-icon-512.png` | 512×512 | Android, PWA. |
| `png/app-icon-192.png` | 192×192 | PWA manifest. |
| `png/apple-touch-icon-180.png` | 180×180 | iPhone. |
| `png/apple-touch-icon-167.png` | 167×167 | iPad Pro. |
| `png/apple-touch-icon-152.png` | 152×152 | iPad. |
| `png/apple-touch-icon-120.png` | 120×120 | iPhone legacy. |

### Социальные превью

| Файл | Размер | Применение |
|---|---|---|
| `numkit-social-en.svg` | 1200×630 | Open Graph, Twitter — английский таглайн. |
| `numkit-social-ru.svg` | 1200×630 | Open Graph, Twitter — русский таглайн. |
| `numkit-social-github.svg` | 1280×640 | GitHub social preview (репозиторий). |
| `numkit-twitter-header.svg` | 1500×500 | Twitter/X профильный баннер. |
| `png/social-en.png` | 1200×630 | Растр для meta-тегов. |
| `png/social-ru.png` | 1200×630 | Русский растр. |
| `png/social-github.png` | 1280×640 | GitHub settings → Social preview. |
| `png/twitter-header.png` | 1500×500 | Twitter settings → Header image. |

---

## Минимальные размеры

| Версия | Минимум | Комментарий |
|---|---|---|
| Полный логотип `[nₖ] numkit` | 200px | Ниже — wordmark сливается. |
| Только знак `[nₖ]` | 32px | Ниже — субскрипт `k` не читается. |
| Favicon `[n]` | 16px | Упрощённая версия без субскрипта. |

---

## Зона безопасности

Вокруг знака/логотипа оставляй отступ не менее **половины высоты скобок**
(примерно 80px при основном размере 160px).

Не размещай в этой зоне: текст, иконки, края фотографий, обрезы экрана.

---

## Правила применения

### Что МОЖНО

- Использовать на белом, очень светлом (до `#f8fafc`) и на `#0f172a` (тёмный режим) фонах
- Масштабировать пропорционально
- Инвертировать в белый на индиго-фоне (app icon)
- Использовать mark-only в квадратных контекстах (аватарки, иконки)

### Что НЕЛЬЗЯ

- ❌ Менять цвет знака на что-то кроме `#6366f1`, `#818cf8` или белого
- ❌ Растягивать или сжимать непропорционально
- ❌ Добавлять тени, градиенты, блики, обводку
- ❌ Отделять скобки от содержимого
- ❌ Убирать субскрипт `k` из основного знака (можно только в фавиконе)
- ❌ Наклонять
- ❌ Использовать wordmark без знака в официальных контекстах (в теле документации
  можно просто писать `numkit` текстом)
- ❌ Помещать на загруженные фотографические фоны

---

## Подключение на сайте

### Favicon в `<head>`

```html
<link rel="icon" href="/favicon.svg" type="image/svg+xml">
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="apple-touch-icon" href="/apple-touch-icon-180.png">
<link rel="apple-touch-icon" sizes="167x167" href="/apple-touch-icon-167.png">
<link rel="apple-touch-icon" sizes="152x152" href="/apple-touch-icon-152.png">
<link rel="apple-touch-icon" sizes="120x120" href="/apple-touch-icon-120.png">
```

### PWA manifest (`manifest.json`)

```json
{
  "name": "numkit",
  "short_name": "numkit",
  "description": "compute and explore, anywhere",
  "start_url": "/",
  "display": "standalone",
  "theme_color": "#6366f1",
  "background_color": "#ffffff",
  "icons": [
    { "src": "/app-icon-192.png", "sizes": "192x192", "type": "image/png" },
    { "src": "/app-icon-512.png", "sizes": "512x512", "type": "image/png" }
  ]
}
```

### Open Graph / Twitter Card

```html
<meta property="og:title" content="numkit">
<meta property="og:description" content="compute and explore, anywhere">
<meta property="og:image" content="https://numkit.dev/social-en.png">
<meta property="og:url" content="https://numkit.dev">
<meta property="og:type" content="website">

<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="numkit">
<meta name="twitter:description" content="compute and explore, anywhere">
<meta name="twitter:image" content="https://numkit.dev/social-en.png">
```

### GitHub README

```markdown
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./assets/numkit-logo-dark.svg">
    <img src="./assets/numkit-logo-light.svg" alt="numkit" width="400">
  </picture>
</p>
```

---

## Регенерация файлов

Все SVG — исходники, сгенерированы из TTF-шрифтов через Python-скрипт
`generate.py` с использованием `fontTools`.

### PNG из SVG

```bash
# rsvg-convert (лучшее качество на мелких размерах)
rsvg-convert -w 512 numkit-app-icon.svg -o app-icon-512.png

# или ImageMagick
convert -background none -density 300 numkit-favicon.svg \
  -resize 256x256 favicon-256.png
```

### Multi-size ICO (Windows)

```bash
convert favicon-16.png favicon-32.png favicon-64.png favicon-256.png favicon.ico
```

---

## Домены и DNS

### numkit.dev — основной

```
A    @      <IP>
AAAA @      <IPv6>
A    www    <IP>
```

### numkit.io — редирект

Настрой 301-редирект на уровне хостинга:

```
numkit.io/*      → https://numkit.dev/$1  (301 permanent)
www.numkit.io/*  → https://numkit.dev/$1  (301 permanent)
```

На Cloudflare: Rules → Page Rules → `numkit.io/*` → Forwarding URL 301 → `https://numkit.dev/$1`.

---

*Этот guide — живой документ. Обновляй при изменениях в визуальной системе.
Исходники — `generate.py` + `/fonts/*.ttf`.*
