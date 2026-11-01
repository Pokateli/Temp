# Флюгегехаймен — лаборатория WireGuard over WSS

Этот комплект использует **оригинальный [erebe/wstunnel](https://github.com/erebe/wstunnel)** (BSD-3-Clause), а не новый/несовместимый протокол.

**Статус: лабораторный комплект.** Код интеграции подготовлен, но реальное соединение ChatGPT → MCP → хост этим коммитом не устанавливается: MCP-клиент чата не предоставляет файловый дескриптор TCP/WebSocket сетевому процессу песочницы.

Анализ совместимости Site: [SITE_COMPATIBILITY.md](SITE_COMPATIBILITY.md).

## Закреплённая зависимость

- Upstream: https://github.com/erebe/wstunnel
- Версия: **v11.1.0** (19 октября 2026).
- Linux AMD64, SHA-256 архива: `512edf4e7bb64fc133522220123514868fae75b5bae4124b53e396787276aa23`
- Linux ARM64, SHA-256 архива: `148895f957a68be39e2b65d28c5f2065b9c58c4a0cfd48c560b2b3267f398464`
- Источник SHA-256: поле `digest` в официальном GitHub Release v11.1.0.
- В репозиторий **не загружаются бинарники и артефакты Actions**.

Установка (на узлах с доступом к GitHub):

```bash
bash scripts/install-wstunnel.sh "$HOME/.local/bin"
"$HOME/.local/bin/wstunnel" --version
```

Проверка WSS + UDP без удалённого хоста (понадобятся `python3`, `openssl`, `wstunnel`):

```bash
WSTUNNEL_BIN="$HOME/.local/bin/wstunnel" bash scripts/smoke-wss-udp.sh
```

Тест создаёт самоподписанный краткосрочный TLS-сертификат и только локальные соединения 127.0.0.1, затем проверяет двусторонний UDP echo через WSS. В **тесте** TLS-проверка сертификата со стороны клиента отключена по умолчанию в wstunnel; в реальной конфигурации включайте её явно.

## Дополнительные проверки и работа без доступа песочницы к GitHub

Исправлена ошибка в имени дистрибутива: `wstunnel_${VERSION}_${TARGET}.tar.gz` (без обратной косой черты перед `_`).

При отсутствии прямого сетевого доступа на машине, где устанавливается клиент, можно **предварительно** скачать оригинальный архив на другом компьютере, перенести его безопасным способом и установить с неизменной проверкой SHA-256:

```bash
WSTUNNEL_ARCHIVE=/path/to/wstunnel_11.1.0_linux_amd64.tar.gz \
  bash scripts/install-wstunnel.sh "$HOME/.local/bin"
```

Измерить локальное UDP echo через обычный сокет без wstunnel:

```bash
python3 scripts/benchmark_udp.py --port 15183 --samples 100 --size 1200
```

Полный smoke-тест с установленным wstunnel сначала проверяет двусторонний UDP echo, затем выводит измерения для прямого UDP и UDP-пересылки через WSS. Метрика `goodput_mbit_s` в нём измеряет **последовательные запросы и ответы**, поэтому не является пределом пропускной способности потока.

Проверено в текущей песочнице: синтаксис установщика, отрицательная проверка SHA-256 и запуск измерителя на локальном UDP echo (30/30 успешных ответов). Сам бинарник wstunnel из GitHub Release из песочницы не загружен: полноценный WSS smoke-тест ожидает среды с доступом к бинарнику. Не заявлять его как пройденный.

## WG-over-WSS: сервер (хост, принимающий WSS)

Нужен существующий WireGuard, который слушает UDP/51820, действующий TLS-сертификат и закрытый доступ к UDP/51820 снаружи. Создайте отдельный CA и клиентский сертификат для mTLS.

```bash
wstunnel server \
  --restrict-to 127.0.0.1:51820 \
  --tls-certificate /etc/fluge/tls/fullchain.pem \
  --tls-private-key /etc/fluge/tls/privkey.pem \
  --tls-client-ca-certs /etc/fluge/tls/client-ca.pem \
  wss://0.0.0.0:443
```

Сервер разрешает только пересылку на WG-порт. Клиенты проходят проверку mTLS. Ключи и сертификаты **не сохранять в GitHub**.

## WG-over-WSS: клиент (процесс, способный открыть исходящий WSS)

```bash
wstunnel client \
  --tls-verify-certificate \
  --tls-certificate /etc/fluge/tls/client.pem \
  --tls-private-key /etc/fluge/tls/client-key.pem \
  -L 'udp://127.0.0.1:51821:127.0.0.1:51820?timeout_sec=0' \
  wss://YOUR-HOST.example:443
```

В конфигурации WG-клиента замените `Endpoint` на `127.0.0.1:51821`; штатные WG-ключи остаются прежними. Для начала используйте узкий `AllowedIPs` (только нужную подсеть), чтобы не получить петлю маршрутизации. Для full-tunnel закрепите отдельный маршрут до публичного IP WSS-хоста **вне** WG. Начальный тестовый MTU — 1300; подбирается измерениями.

Для подключения пользовательского пространства ChatGPT необходимо отдельное, подтверждённое средство **постоянного двунаправленного обмена с MCP**, а не просто установка wstunnel на WSS-сервере.

## Архитектура и честное разграничение

```text
(для полноценного IP-туннеля)
Локальный WG/TCP стек -- UDP socket -- wstunnel client
   -- WSS/TLS:443 --> wstunnel server -- UDP:51820 --> WireGuard Server

(для текущего ChatGPT)
ChatGPT -- MCP tool call --> Site
[ЗДЕСЬ НЕТ постоянного бинарного сокета, доступного процессу песочницы]
```

Репозиторий не меняет системные маршруты, не публикует открытый прокси и не создаёт рабочий туннель в песочнице ChatGPT. Нет GitHub Actions и расхода Actions artifact storage.

Официальное руководство: https://github.com/erebe/wstunnel#wireguard-and-wstunnel
