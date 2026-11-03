# Флюгегехаймен: совместимость MCP Site с wstunnel v11.1.0

## Проверенные сведения из исходников upstream

Изучены **именно исходники v11.1.0**, а не несовместимая старая версия.

- [websocket.rs](https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/transport/websocket.rs): клиент создаёт HTTP GET по пути `/{path_prefix}/events` (по умолчанию `/v1/events`) с `Upgrade: websocket`, `Sec-WebSocket-Protocol` и данными маршрутизации.
- [jwt.rs](https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/transport/jwt.rs): в `Sec-WebSocket-Protocol` добавляется `authorization.bearer.<JWT>`; payload содержит `id` (UUID), `p` (тип туннеля), `r` (хост), `rp` (порт).
- [handler_websocket.rs](https://github.com/erebe/wstunnel/blob/v11.1.0/wstunnel/src/tunnel/server/handler_websocket.rs): сервер обрабатывает tunnel request до завершения WebSocket upgrade и отвечает subprotocol `v1`.
- JWT здесь **не эквивалент аутентификации клиента**: сервер декодирует сведения о направлении и сверяет ограничения. Для реального внешнего сервера нужны `--restrict-to`, надёжный TLS, по возможности mTLS и фильтрация доступа.

## Вывод о совместимости

**Стандартного WSS-клиента с произвольными binary frames недостаточно, чтобы гарантированно говорить с wstunnel server.** Клиент должен соблюдать формат WebSocket Upgrade, subprotocol и правила двунаправленного туннельного потока. Простейший Site WebSocket relay без адаптера протокола не является клиентом wstunnel.

Практический минимальный тест проводится двумя **официальными wstunnel-бинарниками** через `scripts/smoke-wss-udp.sh`. Этот тест не проверяет ChatGPT ↔ MCP.

## Отдельная проблема ChatGPT → Site

Модель может вызывать разрешённые инструменты MCP, но **программы песочницы ChatGPT не получают от MCP сетевой socket/file descriptor**. Нельзя подключить их Linux `tun0` или `veth` к MCP без самостоятельного постоянного транспорта. Поэтому связь ChatGPT → MCP → wstunnel server может быть интерфейсом операций, но пока не является прозрачным TCP/IP из песочницы.

Для заявленной конечной цели следующий обязательный тест — **может ли пользовательский процесс песочницы напрямую открыть WSS/HTTPS-канал к опубликованному Site**. Отрицательные результаты предыдущего сетевого аудита сохраняются; этот репозиторий не обходит их.

## План испытаний

1. На произвольном Linux с доступом к официальному архиву: `install-wstunnel.sh`.
2. На этом Linux: `smoke-wss-udp.sh`, сравнить direct UDP и WSS.
3. На целевом хосте (с mTLS): только UDP WireGuard `127.0.0.1:51820`, WSS TCP/443.
4. Со стороны Site: отдельно подтвердить возможность исходящего WebSocket Upgrade **и** реализацию wstunnel-совместимого handshake.
5. Со стороны процесса песочницы: проверить наличие прямого непрерывного транспорта без вызова модели на пакет.
6. Только после пунктов 4–5 исследовать IP/TCP прозрачность и сквозной throughput.

Результаты пунктов 2–6 пока **не подтверждены**.
