#!/usr/bin/env bash
# RQ-03: tamaño de imagen y tiempo desde `docker compose up` hasta la primera petición correcta.
# Tres corridas, imagen construida desde cero (--no-cache), reporta la mediana.
# Probar en una máquina limpia (solo Docker: sin gcc, sin JDK, sin caché).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bench/resultados
SALIDA=bench/resultados/despliegue.csv
echo "corrida,segundos_construccion,segundos_hasta_primera_huella" > "$SALIDA"

for i in 1 2 3; do
  docker compose down -v >/dev/null 2>&1 || true
  t0=$(date +%s.%N)
  docker compose build --no-cache >/dev/null
  t1=$(date +%s.%N)
  docker compose up -d >/dev/null
  until [ "$(curl -s -o /dev/null -w '%{http_code}' localhost:8080/api/huellas/salud)" = "200" ]; do sleep 0.2; done
  t2=$(date +%s.%N)
  printf '%d,%.1f,%.1f\n' "$i" "$(echo "$t1 - $t0" | bc)" "$(echo "$t2 - $t1" | bc)" | tee -a "$SALIDA"
done

echo "--- tamaño de la imagen (RQ-03: < 250 MB) ---"
docker images siliconminds/huella-service:1.0.0 --format '{{.Repository}}:{{.Tag}}  {{.Size}}'
echo "--- capas: lo que cuesta la biblioteca nativa ---"
docker history siliconminds/huella-service:1.0.0 --format '{{.Size}}\t{{.CreatedBy}}' | head -12
echo "--- ¿tiene gcc la imagen final? (debe decir 'no') ---"
docker run --rm --entrypoint bash siliconminds/huella-service:1.0.0 -c 'command -v gcc >/dev/null && echo SI || echo no'
echo "--- Dockerfile de una sola etapa (comparación): ver BITACORA.md D-08 ---"
