# webserv — руководство по сборке, запуску и тестированию

## Итог сессии — что изменилось

| # | Что | Файл(ы) |
|---|-----|---------|
| 1 | Добавлен CGI-обработчик (fork/exec, pipe, env) | `src/http/cgi/` |
| 2 | Директива `cgi` в конфиге (extension → interpreter) | `src/parser/`, `src/config.hpp` |
| 3 | `HttpResponse.headers` для передачи заголовков от CGI | `src/http/types.hpp` |
| 4 | Исправлено чтение POST-запросов (читаем до Content-Length) | `src/event-loop/EventLoop.cpp` |
| 5 | Статический сайт (HTML + CSS) | `www/html/` |
| 6 | Тестовые CGI-скрипты на Python | `www/cgi-bin/` |
| 7 | Конфиг для запуска | `configs/config_cgi.conf` |
| 8 | Кастомные HTML-страницы ошибок (400/403/404/405/500/504) | `www/html/errors/` |
| 9 | Директива `error_page <code> <path>` в конфиге | `src/parser/`, `src/config.hpp` |

---

## Что было реализовано в этой сессии

### 1. Обработчик CGI (`src/http/cgi/`)

Добавлена поддержка CGI/1.1 через директиву `cgi` в блоках `location`.  
Сервер определяет CGI-запрос по расширению файла, затем:

1. Разбирает путь запроса на `SCRIPT_NAME` и `QUERY_STRING`.
2. Выставляет переменные окружения по спецификации CGI/1.1.
3. Делает `fork()` + `execve()` с двумя pipe-ами (stdin/stdout).
4. Пишет тело запроса в stdin CGI-процесса.
5. Читает stdout через `select()` с таймаутом 30 секунд.
6. Парсит CGI-ответ (заголовки + тело) и передаёт клиенту.

Новые файлы:
```
src/http/cgi/
├── CgiException.hpp   — исключение для ошибок CGI
├── CgiHandler.hpp     — заголовок обработчика
└── CgiHandler.cpp     — реализация (fork/exec, pipe, env)
```

### 2. Изменения в существующих файлах

| Файл | Что изменилось |
|------|----------------|
| `src/config.hpp` | В `Location` добавлено поле `cgiHandlers` (`map<ext, interpreter>`) |
| `src/http/types.hpp` | В `HttpResponse` добавлено поле `headers` для передачи заголовков от CGI |
| `src/parser/Parser.hpp` | Добавлен define `CGI_DIRECTIVE "cgi"` |
| `src/parser/Parser.cpp` | Директива `cgi` принимает два значения: расширение и интерпретатор |
| `src/validator/Validator.cpp` | `index` теперь не обязателен, если у локации есть CGI-обработчики |
| `src/http/response-manager/ResponseManager.cpp` | Определение CGI по расширению файла; передача заголовков из `HttpResponse.headers` |

### 3. Статический сайт (`www/html/`)

```
www/
├── html/
│   ├── index.html     — главная страница
│   ├── about.html     — описание архитектуры
│   ├── contact.html   — форма (отправляет POST на CGI)
│   ├── 404.html       — страница ошибки
│   └── style.css      — общие стили
└── cgi-bin/
    ├── hello.py       — GET: выводит имя из QUERY_STRING
    ├── post.py        — POST: читает тело запроса и выводит его
    └── env.py         — GET: выводит все CGI переменные окружения
```

---

## Синтаксис конфигурации

### Директива `cgi`

```nginx
location /cgi-bin {
    root /path/to/www;
    cgi .py  /usr/bin/python3;
    cgi .sh  /bin/sh;
    cgi .php /usr/bin/php-cgi;
}
```

- Первый аргумент — расширение файла (с точкой).
- Второй аргумент — абсолютный путь к интерпретатору.
- В одной location можно задать несколько расширений.

### Переменные окружения, которые выставляет сервер

| Переменная | Значение |
|---|---|
| `GATEWAY_INTERFACE` | `CGI/1.1` |
| `SERVER_PROTOCOL` | из запроса (`HTTP/1.1`) |
| `SERVER_SOFTWARE` | `webserv/1.0` |
| `SERVER_NAME` | `localhost` |
| `SERVER_PORT` | порт сервера |
| `REQUEST_METHOD` | `GET`, `POST`, … |
| `QUERY_STRING` | часть URL после `?` |
| `SCRIPT_NAME` | URL-путь к скрипту |
| `SCRIPT_FILENAME` | полный путь в файловой системе |
| `CONTENT_TYPE` | из заголовка запроса |
| `CONTENT_LENGTH` | размер тела |
| `REDIRECT_STATUS` | `200` (нужно для php-cgi) |
| `HTTP_*` | все остальные HTTP-заголовки |

---

## Сборка

```bash
make          # собрать
make re       # пересобрать с нуля
make clean    # удалить объектные файлы
make fclean   # удалить объектные файлы и бинарник
```

---

## Запуск

```bash
./webserv configs/config_cgi.conf
```

Конфиг поднимает сервер на порту **9090**.

---

## Тестирование статики

### Через браузер

Открыть `http://localhost:9090/` — должна открыться главная страница со стилями и навигацией.

### Через curl

```bash
# Главная страница (200)
curl -v http://localhost:9090/

# About (200)
curl -s http://localhost:9090/about.html

# Contact (200)
curl -s http://localhost:9090/contact.html

# CSS-файл (200)
curl -s http://localhost:9090/style.css | head -5

# Несуществующая страница (404)
curl -v http://localhost:9090/nonexistent.html
```

### Через Python (если curl недоступен)

```python
import urllib.request

pages = ['/', '/about.html', '/contact.html', '/style.css', '/404.html', '/nonexistent']
for path in pages:
    try:
        r = urllib.request.urlopen('http://localhost:9090' + path)
        print(r.status, path)
    except Exception as e:
        print(getattr(e, 'code', str(e)), path)
```

Ожидаемые коды ответа:

| URL | Код |
|-----|-----|
| `/` | 200 |
| `/about.html` | 200 |
| `/contact.html` | 200 |
| `/404.html` | 200 (сам файл существует) |
| `/nonexistent` | 404 |

---

## Тестирование CGI

### GET-запрос с query string

```bash
curl "http://localhost:9090/cgi-bin/hello.py?name=World"
```

Ожидается HTML с `Hello, World!`.

### Просмотр переменных окружения

```bash
curl "http://localhost:9090/cgi-bin/env.py"
```

Вернёт таблицу со всеми CGI-переменными, которые сервер передал скрипту.

### POST-запрос с телом

```bash
curl -X POST \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -d "name=Alice&message=Hello" \
     http://localhost:9090/cgi-bin/post.py
```

Скрипт выведет тело запроса обратно в HTML.

### Добавить PHP-CGI (если установлен)

1. Установить `php-cgi`:
   ```bash
   sudo apt-get install php-cgi   # Debian/Ubuntu
   ```

2. Добавить в `configs/config_cgi.conf`:
   ```nginx
   location /cgi-bin {
       root /home/codeex/webserv/www;
       cgi .py  /usr/bin/python3;
       cgi .sh  /bin/sh;
       cgi .php /usr/bin/php-cgi;
   }
   ```

3. Создать скрипт `www/cgi-bin/hello.php`:
   ```php
   <?php
   header("Content-Type: text/html");
   echo "<h1>Hello from PHP!</h1>";
   echo "<p>Method: " . $_SERVER['REQUEST_METHOD'] . "</p>";
   ?>
   ```

4. Обратиться:
   ```bash
   curl http://localhost:9090/cgi-bin/hello.php
   ```

---

## cgi_tester

`cgi_tester` — это Go-бинарник, который сам является CGI-программой. Сервер должен его **запустить как CGI-скрипт** (не использовать напрямую).

Скопируйте его в `www/cgi-bin/` и добавьте в конфиг расширение без расширения через wrapper-скрипт, или просто запустите его через `cgi_tester`-локацию. Чтобы он сработал, сервер должен выставить `PATH_INFO` (сейчас пустой — нужно будет уточнить ожидаемый формат вызова).

---

## Структура проекта после изменений

```
webserv/
├── configs/
│   ├── config_cgi.conf          ← основной конфиг (CGI + статика, порт 9090)
│   ├── config_basic.conf        ← минимальный конфиг (порт 8080, только статика)
│   └── config_*.conf            ← тестовые конфиги (ошибки парсера, мульти-сервер)
├── src/
│   ├── config.hpp               ← структуры Server, Location (+ cgiHandlers)
│   ├── http/
│   │   ├── types.hpp            ← HttpRequest, HttpResponse (+ headers)
│   │   ├── cgi/                 ← NEW: CGI обработчик
│   │   │   ├── CgiException.hpp
│   │   │   ├── CgiHandler.hpp
│   │   │   └── CgiHandler.cpp
│   │   ├── request-parser/      ← парсер HTTP-запросов
│   │   ├── response-manager/    ← ResponseManager (статика + CGI-диспатч)
│   │   └── router/              ← longest-prefix роутер
│   ├── event-loop/              ← epoll event loop
│   ├── parser/                  ← парсер конфига (+ директива cgi)
│   ├── validator/               ← валидатор конфига
│   └── ...
└── www/
    ├── html/
    │   ├── index.html
    │   ├── about.html
    │   ├── contact.html
    │   ├── 404.html
    │   └── style.css
    └── cgi-bin/
        ├── hello.py
        ├── post.py
        └── env.py
```
