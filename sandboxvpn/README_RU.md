# SandboxVPN v0.1.0 — пользовательский транспорт специально для ChatGPT sandbox

**Проверено локально:** SOCKS5 CONNECT TCP и UDP datagram с фиксированным назначением поверх одного WSS сеанса; TLS, Bearer-token, ограниченный список TCP-назначений, переподключение. Автоматические тесты находятся в `tests/`.

**Не является:** интерфейсом TUN, полноценной L3 VPN, самостоятельным WireGuard/AWG handshake-клиентом, обходом сетевых ограничений или прямым доступом к инструментам MCP из Python.

## Почему так

Песочница ChatGPT в тестируемой сессии предоставляет сокеты loopback и Python 3.11+, но не предоставляет `/dev/net/tun`, а DNS и исходящие TCP/WSS запросы не работают. Поэтому полноценная прозрачная VPN-сеть с системными маршрутами в ней не запустится. Локальные приложения, умеющие работать с SOCKS5, могут использовать транспорт **только в среде, где разрешён исходящий WSS**.

В текущем ChatGPT коннекторе «Флюгегехаймен» есть инструменты `list_hosts`, `host_status`, `service_status`, `transport_exchange`. Инструмент `transport_sequence` недоступен для вызова в данной сессии. `transport_exchange` осуществляет отдельный WSS-обмен, а не предоставляет живой сокет Python-процессу. Для автоматической связи между Python и MCP нужен отдельный поддерживаемый bridge/stream tool. Режим `mcp_handoff` лишь создаёт безопасный ручной JSON-line интерфейс для экспериментов, **не автоматизирует вызов инструмента**.

## Архитектура

```text
Приложения внутри sandbox
  |- SOCKS5/TCP 127.0.0.1:1080
  `- UDP datagrams 127.0.0.1:51820 (WG/AWG packet carrier)
           |
    SandboxVPN client (user-space, no root)
           | WSS/443 + TLS validation + Bearer auth
    SandboxVPN gateway (on trusted external host)
           |- TCP only to destinations explicitly allowlisted
           `- UDP only to one configured endpoint
                 e.g. 37.110.89.86:6269
```

В текущем ограниченном sandbox линия WSS **не устанавливается** (внешний egress запрещён). Только loopback TCP/UDP был проверен. Поддержка вызовов MCP модели не равна исходящей сети контейнера.

## Установка

Python 3.11+; нужна зависимость `websockets>=14,<17`.

```bash
python3 -m pip install .
export SANDBOXVPN_TOKEN="$(python3 -c 'import secrets;print(secrets.token_urlsafe(48))')"
```

Не сохранять токен в Git или в логах; одинаковый токен должен быть сконфигурирован на клиенте и шлюзе через переменную окружения.

## Gateway на Debian

Пример: TLS с Cloudflare Tunnel/reverse proxy (внешний `wss://` завершает TLS на Cloudflare, внутренний ws:// доступен **только через loopback**):

```bash
sandboxvpn-gateway --listen 127.0.0.1 --port 8765 --allow-ws-local \
  --udp-target 37.110.89.86:6269 \
  --allow-tcp 127.0.0.1:22
```

Cloudflare Tunnel должен проксировать на `http://127.0.0.1:8765` и требовать HTTPS для клиентов. Устанавливать шлюз на 0.0.0.0 без TLS нельзя, CLI блокирует этот вариант. TCP назначение `127.0.0.1:22` указано только как пример: разрешайте исключительно свои хосты/порты. Если SSH не нужен — не задавайте `--allow-tcp`. Внешняя защищённая поверхность должна требовать аутентификацию Cloudflare Access или эквивалентную дополнительную авторизацию.

Либо включить собственный TLS:

```bash
sandboxvpn-gateway --listen 0.0.0.0 --port 8765 \
  --tls-cert /etc/sandboxvpn/fullchain.pem --tls-key /etc/sandboxvpn/privkey.pem \
  --udp-target 37.110.89.86:6269
```

## Клиент

```bash
sandboxvpn-client --url wss://your-gateway.example/vpn \
  --socks-port 1080 --udp-port 51820
```

SOCKS5: `socks5h://127.0.0.1:1080` (DNS выполняется на шлюзе только для явно разрешённого имени). UDP: приложения посылают датаграммы в `127.0.0.1:51820`; все датаграммы с одного локального `(IP,port)` передаются в **одну WSS-сессию** и через **один upstream UDP-сокет**, сохраняя один source port до разрыва WSS. Удобно для режима WireGuard/AWG без TUN, использующего userspace `conn.Bind`, однако этот набор сам не реализует WG/AWG и не подменяет полноценный IP-стек (`netstack`). После переподключения сессии и сопоставление портов создаются заново — WG/AWG должен инициировать новую сессию.

## MCP ручной шлюз (только для диагностики)

```bash
sandboxvpn-mcp-handoff --udp-port 51820
```

Программа читает локальные UDP-пакеты и выводит JSON в stdout:

```json
{"seq":1,"tool":"transport_exchange","payload_base64":"AAECAw=="}
```

Оператор вызывает одноимённый MCP инструмент и, **только если в ответе реально есть исходные двоичные данные**, отправляет строку в stdin:

```json
{"seq":1,"response_base64":"BAUG"}
```

В настоящий момент `transport_exchange` возвращает диагностический результат, а не подтверждённый VPN-ответ: получено `wss_connected=true`, `packet_sent=true`, `response_timeout=true`, `udp_forwarded=null`. Следовательно, **по MCP туннель пока не функционирует**. Он не обеспечит WG/AWG handshake даже если UDP-пакет отправлен в WSS.

## Проверки / безопасность

```bash
python3 -m pip install -e '.[test]'
pytest -q
```

- WSS (`wss://`) обязателен вне loopback. Сертификат проверяется; `--ca-file` поддерживает доверенный частный CA.
- Ключ доступа не менее 32 символов, передаётся HTTP Authorization поверх TLS, сравнивается constant-time на шлюзе.
- TCP: default-deny по точному `(host,port)`; UDP: **только один** заданный сервером адрес:порт.
- Сторонние приложения могут использовать SOCKS5 CONNECT; UDP ASSOCIATE в SOCKS5 не реализован: для WG/AWG отдельный локальный UDP-порт.
- Ограничение до 128 одновременных каналов на сессию и до 65507 байт на сообщение.
- Аутентификация и политика адресов не заменяют защиту конечных удалённых сервисов. Для production нужны rate-limits, мониторинг и ограничения доступности.
- Тесты проверяют localhost; аппаратная маршрутизация и внешний WSS в текущей сессии не подтверждены.

## Версия и ограничения

v0.1.0 (proof-of-concept). Не передавать пароли или чувствительные данные через непроверенные публичные шлюзы. Использовать только на серверах, которыми вы управляете.

