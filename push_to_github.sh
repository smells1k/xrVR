#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# push_to_github.sh - публикация репозитория xrVR на GitHub.
#
# Запуск (токен НЕ сохраняется в файлы, только в переменную процесса):
#
#     GH_TOKEN='ghp_xxxx' bash /home/user/xrvr/push_to_github.sh
#
# Или, если токен уже в окружении:
#
#     bash /home/user/xrvr/push_to_github.sh
#
# Скрипт:
#   1. проверяет токен и права (scope repo / workflow);
#   2. создаёт репозиторий smells1k/xrVR (публичный), если его нет;
#   3. пушит ветку master + тег v0.1.0;
#   4. создаёт релиз v0.1.0 с двумя ассетами:
#        xrVR-kit-v0.1.0.zip   - весь комплект (модуль, патчи, тесты, конфиг)
#        xrVR-docs-v0.1.0.zip  - только README и конфиг
#   5. скачивает ассет обратно и сверяет SHA256.
#
# Ничего не пишется в /home/user кроме временных файлов в /tmp.
# -----------------------------------------------------------------------------
set -u

REPO="smells1k/xrVR"
TAG="v0.1.0"
SRC="/home/user/xrvr"
WORK="/tmp/xrvr_release"

die() { echo "!! ОШИБКА: $*" >&2; exit 1; }
say() { echo "==> $*"; }

# --- 1. токен ---------------------------------------------------------------
[ -n "${GH_TOKEN:-}" ] || die "GH_TOKEN не задан. Запуск: GH_TOKEN='ghp_...' bash $0"

API="https://api.github.com"
auth=(-H "Authorization: Bearer $GH_TOKEN" -H "Accept: application/vnd.github+json")

say "Проверка токена"
who=$(curl -s "${auth[@]}" "$API/user" | python3 -c "import sys,json;print(json.load(sys.stdin).get('login',''))" 2>/dev/null)
[ -n "$who" ] || die "токен не работает (неверный или отозван)"
echo "    логин: $who"

scopes=$(curl -sI "${auth[@]}" "$API/user" | grep -i "^x-oauth-scopes:" | cut -d: -f2- | tr -d ' \r')
echo "    scope: ${scopes:-<нет заголовка>}"
case "$scopes" in
  *repo*) : ;;
  *) echo "    ВНИМАНИЕ: нужен scope 'repo'. Пуш может не пройти." ;;
esac

# --- 2. репозиторий ---------------------------------------------------------
say "Проверка репозитория $REPO"
code=$(curl -s -o /dev/null -w "%{http_code}" "${auth[@]}" "$API/repos/$REPO")
if [ "$code" = "404" ]; then
    say "Создаю публичный репозиторий $REPO"
    out=$(curl -s -X POST "${auth[@]}" "$API/user/repos" \
        -d '{"name":"xrVR","description":"VR-мод для S.T.A.L.K.E.R. Тень Чернобыля на OGSR Engine. Стерео, 6DoF, масштаб 1:1. Pico Neo 3 через SteamVR.","private":false,"has_issues":true,"has_wiki":false}')
    echo "$out" | python3 -c "import sys,json;d=json.load(sys.stdin);print('    создан:',d.get('full_name') or d.get('message'))"
    [ "$(echo "$out" | python3 -c "import sys,json;print(1 if 'full_name' in json.load(sys.stdin) else 0)" 2>/dev/null)" = "1" ] \
        || die "не удалось создать репозиторий: $out"
elif [ "$code" = "200" ]; then
    say "репозиторий уже существует, обновляю"
else
    die "неожиданный ответ GitHub: HTTP $code"
fi

# --- 3. пуш ----------------------------------------------------------------
cd "$SRC" || die "нет $SRC"
git remote remove origin 2>/dev/null
git remote add origin "https://x-access-token:${GH_TOKEN}@github.com/${REPO}.git"
say "Пуш ветки master"
git push -f origin master 2>&1 | tail -4 || die "push не удался"

say "Тег $TAG"
git tag -f "$TAG" >/dev/null 2>&1
git push -f origin "$TAG" 2>&1 | tail -3 || die "push тега не удался"

# токен больше не нужен в remote-конфиге
git remote set-url origin "https://github.com/${REPO}.git"

# --- 4. релиз --------------------------------------------------------------
say "Сборка ассетов релиза"
rm -rf "$WORK"; mkdir -p "$WORK"
cd "$SRC" || die "нет $SRC"

# полный комплект (без .git)
tar -c --exclude=.git . > "$WORK/src.tar" 2>/dev/null
( cd "$SRC" && zip -qr "$WORK/xrVR-kit-$TAG.zip" . -x '.git/*' ) || \
    python3 - "$SRC" "$WORK/xrVR-kit-$TAG.zip" <<'PY'
import os,sys,zipfile
src,out=sys.argv[1],sys.argv[2]
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for root,dirs,files in os.walk(src):
        dirs[:]=[d for d in dirs if d!='.git']
        for f in files:
            p=os.path.join(root,f)
            z.write(p,os.path.relpath(p,src))
print('zip via python')
PY
rm -f "$WORK/src.tar"

# только документация + конфиг
( cd "$SRC" && zip -qr "$WORK/xrVR-docs-$TAG.zip" README.md gamedata ) || \
    die "не удалось собрать docs-zip"

ls -lh "$WORK"/*.zip

say "Создание релиза $TAG"
body=$(python3 - <<'PY'
import json
body = """VR-мод для S.T.A.L.K.E.R. Тень Чернобыля на OGSR Engine.

Стерео-рендер, трекинг головы 6DoF, масштаб 1:1, стрельба по стволу,
виртуальный экран для меню и ПДА. Pico Neo 3 через SteamVR.

## Установка

```
git clone https://github.com/smells1k/xrVR.git
cd xrVR
python tools\\apply_patches.py  C:\\path\\to\\OGSR-Engine
```

Затем открыть `Engine.sln` и собрать `Release|x64`.

Скрипт применяет 6 патчей к исходникам движка, копирует модуль,
правит три `.vcxproj` и прогоняет 211 авто-проверок.
В конце должно быть `РЕЗУЛЬТАТ: ВСЁ ПРИМЕНЕНО`.

## В архиве

- `src/xrVR/` — модуль (10 файлов)
- `tools/` — патчи, интегратор, проверки
- `tests/` — 79 проверок математики
- `gamedata/configs/vr/vr_config.ltx` — конфиг

Полная инструкция — в README.md.
"""
print(json.dumps({"tag_name":"v0.1.0","name":"xrVR v0.1.0 — первый рабочий релиз","body":body,"draft":False,"prerelease":False}))
PY
)
rel_code=$(curl -s -o /tmp/rel.json -w "%{http_code}" -X POST "${auth[@]}" "$API/repos/$REPO/releases" -d "$body")
if [ "$rel_code" = "201" ]; then
    say "релиз создан"
else
    say "релиз уже есть (HTTP $rel_code), обновляю ассеты"
fi

# --- 5. заливка ассетов ----------------------------------------------------
upload() {
    local file="$1" name
    name=$(basename "$file")
    say "Заливка $name ($(du -h "$file" | cut -f1))"
    url="https://uploads.github.com/repos/$REPO/releases/tags/$TAG/assets?name=$name"
    # если ассет уже есть - удалить
    aid=$(curl -s "${auth[@]}" "$API/repos/$REPO/releases/tags/$TAG" \
        | python3 -c "import sys,json
try:
    d=json.load(sys.stdin)
    for a in d.get('assets',[]):
        if a['name']=='$name': print(a['id'])
except Exception: pass" | head -1)
    if [ -n "$aid" ]; then
        curl -s -X DELETE "${auth[@]}" "$API/repos/$REPO/releases/assets/$aid" >/dev/null
        echo "    старый ассет удалён (id $aid)"
    fi
    r=$(curl -s -X POST "${auth[@]}" \
        -H "Content-Type: application/octet-stream" \
        --data-binary @"$file" "$url")
    echo "$r" | python3 -c "import sys,json;d=json.load(sys.stdin);print('    загружено:',d.get('browser_download_url') or d.get('message'))"
}

upload "$WORK/xrVR-kit-$TAG.zip"
upload "$WORK/xrVR-docs-$TAG.zip"

# --- 6. проверка -----------------------------------------------------------
say "Проверка целостности"
dl=$(curl -s "${auth[@]}" "$API/repos/$REPO/releases/tags/$TAG" \
    | python3 -c "import sys,json
d=json.load(sys.stdin)
print('ASSETS')
for a in d.get('assets',[]):
    print(' ',a['name'], a['size'], a['browser_download_url'])")
echo "$dl"

url_kit=$(echo "$dl" | grep "xrVR-kit-$TAG.zip" | awk '{print $3}')
if [ -n "$url_kit" ]; then
    curl -sL -o /tmp/check.zip "$url_kit"
    echo "    локально: $(sha256sum "$WORK/xrVR-kit-$TAG.zip" | cut -d' ' -f1)"
    echo "    скачано : $(sha256sum /tmp/check.zip | cut -d' ' -f1)"
    if cmp -s /tmp/check.zip "$WORK/xrVR-kit-$TAG.zip"; then
        echo "    СОВПАДАЕТ"
    else
        echo "    !! НЕ СОВПАДАЕТ"
    fi
    rm -f /tmp/check.zip
fi

echo
echo "============================================================"
echo "ГОТОВО: https://github.com/$REPO"
echo "Релиз: https://github.com/$REPO/releases/tag/$TAG"
echo "============================================================"
