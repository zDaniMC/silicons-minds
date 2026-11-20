#!/usr/bin/env bash
# RQ-01 en el entorno que declara el manifiesto (contenedor de 128 MB, una sola petición a la vez).
# Corre el banco dentro del contenedor y, en paralelo, muestrea `docker stats` cada segundo para
# comprobar que la memoria se mantiene plana (si crece con el archivo, la lectura por bloques falla).
# Resultados crudos: bench/resultados/banco.csv y bench/resultados/memoria.csv
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bench/resultados
docker compose build
NOMBRE=banco-huella
docker rm -f "$NOMBRE" >/dev/null 2>&1 || true
echo "tiempo,uso_memoria" > bench/resultados/memoria.csv

docker compose run --rm --name "$NOMBRE" --entrypoint java huella-service \
  -Dloader.main=co.siliconminds.huella.BancoMedicion -cp /app/huella-service.jar \
  org.springframework.boot.loader.launch.PropertiesLauncher "$@" \
  | tee bench/resultados/banco.csv &
PID=$!

while kill -0 "$PID" 2>/dev/null; do
  uso=$(docker stats --no-stream --format '{{.MemUsage}}' "$NOMBRE" 2>/dev/null || true)
  [ -n "$uso" ] && echo "$(date +%T),$uso" >> bench/resultados/memoria.csv
  sleep 1
done
wait "$PID"
echo "--- memoria (docker stats) ---"
cat bench/resultados/memoria.csv
