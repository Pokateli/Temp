# Tiny11 Ready VM — два режима запуска (GitHub / ChatGPT)

## Цель

Создать **один раз** полностью установленный Windows 11 Enterprise Evaluation (Tiny11)
в форме самостоятельного диска **tiny11-ready-zstd.qcow2**, уже прошедшего OOBE.
При следующих запусках **никакого ISO, DISM, Tiny11 Builder, повторной установки Windows
или новой учётной записи не требуется**. Исходный сжатый QCOW2 неизменяемый;
каждая сессия использует отдельный qcow2 overlay.

Этот проект находится **только в Pokateli/Temp**, ветка
`windows-tiny11-dual-ready-20261029`. Основная ветка Temp не затрагивается.

### Проверяемая структура готового комплекта

```text
tiny11-ready/
  tiny11-ready-zstd.qcow2       # УЖЕ установленная и настроенная Windows
  tiny11-ready-manifest.json    # SHA-256, виртуальный размер, состояние OOBE
  OVMF_CODE.fd                  # UEFI firmware
  OVMF_VARS.fd                  # UEFI NVRAM после подготовки
  SHA256SUMS
  ready-credentials.txt         # Секретный пароль локальной учётной записи vmuser
  tiny11-session.qcow2          # создаётся при первом запуске, не входит в базовый пакет
```

Размер готового диска пока не подтверждён: более ранний **установленный, но
ещё не завершивший OOBE** образ занимал около 7,9 ГБ со сжатием ZSTD.
Размер после OOBE измеряется новой сборкой.

## Вариант 1: GitHub — собрать один раз

[Workflow «Tiny11 prepare-once ready disk»](../../actions/workflows/tiny11-ready-build.yml)

1. Скачивает официальный Microsoft ISO и проверяет опубликованный SHA-256.
2. NTDev Tiny11 Builder подготавливает облегчённый install.wim.
3. DISM применяет WIM к UEFI VHDX и настраивает автономную локальную учётную запись **vmuser**
   с уникальным случайным паролем. Пароль не выводится в журналы.
4. QEMU TCG **первый раз** запускает Windows и ожидает контрольного файла
   **TINY11_FIRST_LOGON_READY.TXT** от самой гостевой системы. При первой настройке
   виртуальная машина автоматически выполняет **штатное выключение**.
5. Из изменённого слоя создаётся самостоятельный **tiny11-ready-zstd.qcow2**.
   Проверяется идентичность данных, отсутствие backing-файла, SHA-256 и ZSTD.
6. Workflow второй раз запускает **уже установленный** образ и ожидает
   **TINY11_INSTALLED_READY.TXT** от службы проверки при следующей загрузке.

**Важно:** если первый вход, выключение или повторный запуск не подтверждены,
workflow должен завершиться с ошибкой и не выдавать диск за готовый.

### Передача готового диска из публичного Temp

Поскольку `Temp` публичный и диск содержит лицензируемую Windows и учётные
данные, **незашифрованный Windows-образ никогда не загружается в GitHub Artifact**.

Для включения доставки пользователь должен создать секрет GitHub Actions:

**Repository → Settings → Secrets and variables → Actions → New repository secret**.

Имя: `TINY11_ARCHIVE_PASSPHRASE`. Значение: длинная приватная фраза **от 16 символов**.
Не записывайте её в GitHub исходники или в чат. Храните свой экземпляр безопасно.

Если секрет установлен, workflow создаст artifact `tiny11-ready-encrypted-v1`:
AES-256-GCM, scrypt, аутентифицированные блоки 128 МиБ, SHA-256 каждого
зашифрованного блока. В пакет включается **готовый QCOW2, UEFI, манифест и
учётные данные** — всё только в зашифрованном виде.

Без этого секрета Windows может быть собрана и проверена в Actions, но готовый
диск **остаётся лишь на временном Runner** и впоследствии удаляется. Это
ограничение принципиально: успешная сборка сама по себе ещё не означает, что
образ можно скачать в другой чат.

## Вариант 2: GitHub — запуск без сборки

[Workflow «Run preinstalled Tiny11 QCOW2»](../../actions/workflows/tiny11-ready-run.yml).

Укажите **source_run_id** успешной сборки, имеющей artifact
`tiny11-ready-encrypted-v1`. Workflow загружает зашифрованные части,
расшифровывает с помощью секрета GitHub Actions, проверяет манифест и запускает
**готовый Windows-диск** через QEMU TCG. Образ не устанавливается заново.

Проверка успешна только при получении файла от гостевой Windows.

## Вариант 3: запуск в песочнице ChatGPT

После того как через GitHub Connector или пользовательскую передачу зашифрованный
artifact будет помещён в папку `/mnt/data/tiny11-ready-encrypted`, а пароль
станет доступен в переменной окружения `TINY11_ARCHIVE_PASSPHRASE`,
и QEMU установлен из нашего Debian 13 portable kit:

```bash
export QEMU_HOME=/mnt/data/debian13-chatgpt-vm
export ENCRYPTED_DIR=/mnt/data/tiny11-ready-encrypted
export READY_DIR=/mnt/data/tiny11-ready

bash windows-vm-runtime/bootstrap-chatgpt.sh verify
bash windows-vm-runtime/bootstrap-chatgpt.sh start
```

Для проверки загрузки без VNC:

```bash
bash windows-vm-runtime/bootstrap-chatgpt.sh probe
```

Для новой чистой сессии без изменений предыдущей:

```bash
bash windows-vm-runtime/bootstrap-chatgpt.sh reset-session
```

Экран: локальный VNC `127.0.0.1:5900`, не внешний IP. Сеть гостя отключена.
Гость по умолчанию получает 2 vCPU, 2560 МиБ RAM, QEMU TCG, OVMF.
Локальный лимит контейнера ChatGPT ~4 ГБ: **фактическую пригодность полностью
загруженной Tiny11 необходимо проверить отдельно**. Нет KVM; скорость Windows
может быть значительно ниже, чем на обычном ПК.

### Безопасность

- Не используйте этот образ как обычный интернет-десктоп: он создан для
  изолированной проверки программ, сеть гостя отключена.
- Шифрование защищает artifact на публичном GitHub. Не публикуйте пароль или
  `ready-credentials.txt` в открытом виде.
- Оценочная Windows регулируется условиями Microsoft, а Tiny11 — сторонняя
  модификация. Возможность загрузки в VM не снимает лицензионных ограничений.
- Сборка в GitHub и запуск на Windows Runner **не доказывают** возможность
  загрузки в песочнице ChatGPT, пока образ не импортирован и не проверен там.

## Диагностика и независимая проверка

[Успешный синтетический тест обеих сред и AES-GCM](../../actions/workflows/tiny11-dual-smoke.yml):
проверены QEMU TCG на Linux, раздельный overlay, отрицательный тест
готовности искусственного диска, AES-256-GCM encrypted transfer и совпадение SHA-256.

Успех первого выпуска настоящего `tiny11-ready-zstd.qcow2` определяется
отдельно по результатам workflow `tiny11-ready-build.yml`, наличию
`windows-first-logon-proof.txt` и `probe-result.json` с
`boot_confirmed: true`.
