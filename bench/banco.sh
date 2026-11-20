#!/usr/bin/env bash
# RQ-01 sin contenedor: C (vía JNA) contra MessageDigest, 3 corridas descartadas + 5 medidas, mediana en MB/s.
# Requiere gcc, JDK 21 y Maven. Uso: bench/banco.sh [archivo] [MB]
set -euo pipefail
cd "$(dirname "$0")/.."
make -C native libhuella.so >/dev/null
mvn -q -B -f huella-service -DskipTests package
exec java -Dstdout.encoding=UTF-8 -Djna.library.path=native \
  -Dloader.main=co.siliconminds.huella.BancoMedicion \
  -cp huella-service/target/huella-service-1.0.0.jar \
  org.springframework.boot.loader.launch.PropertiesLauncher "$@"
