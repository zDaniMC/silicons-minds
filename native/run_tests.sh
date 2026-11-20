#!/usr/bin/env bash
# RQ-02: un solo comando (make test) compila, ejecuta los vectores y compara contra sha256sum.
# No necesita Java, Spring, Docker ni red. Debe correr en menos de 30 segundos.
set -u
cd "$(dirname "$0")"
inicio=$(date +%s.%N)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
casos=0; fallos=0

ok()  { casos=$((casos+1)); echo "  OK     $1"; }
mal() { casos=$((casos+1)); fallos=$((fallos+1)); echo "  FALLA  $1"; }

echo "Referencia: sha256sum contra libhuella (bloques de 64 KB y de 1024 bytes)"
: > "$T/vacio";                      printf 'abc' > "$T/abc"
head -c 55  /dev/urandom > "$T/borde_55"      # el relleno cabe justo
head -c 56  /dev/urandom > "$T/borde_56"      # el relleno obliga a un bloque adicional
head -c 64  /dev/urandom > "$T/borde_64"      # bloque completo; el relleno es un bloque entero
head -c 100003 /dev/urandom > "$T/primo_100003"
head -c 10000000 /dev/urandom > "$T/grande_10MB"
for f in vacio abc borde_55 borde_56 borde_64 primo_100003 grande_10MB; do
  esperado=$(sha256sum "$T/$f" | cut -d' ' -f1)
  for bloque in 65536 1024; do
    obtenido=$(./test_huella hash "$T/$f" $bloque)
    [ "$esperado" = "$obtenido" ] && ok "$f (bloque $bloque) == sha256sum" || mal "$f (bloque $bloque) != sha256sum"
  done
done

echo
echo "Casos autocontenidos"
./test_huella > "$T/salida.txt"; estado=$?
cat "$T/salida.txt"
resumen=$(tail -1 "$T/salida.txt")
internos=$(echo "$resumen" | cut -d' ' -f1)
internos_fallos=$(echo "$resumen" | cut -d' ' -f3)
casos=$((casos + internos)); fallos=$((fallos + internos_fallos))
[ "$estado" -eq 0 ] || fallos=$((fallos > 0 ? fallos : 1))

echo
if command -v valgrind >/dev/null 2>&1; then
  if valgrind -q --leak-check=full --error-exitcode=9 ./test_huella ciclos 2000 2>"$T/vg.txt"; then
    ok "valgrind: 2000 ciclos sin fugas ni accesos inválidos"
  else
    mal "valgrind encontró problemas"; cat "$T/vg.txt"
  fi
else
  echo "  (valgrind no está instalado: la prueba de fugas usa la memoria residente)"
fi

fin=$(date +%s.%N)
dur=$(echo "$fin - $inicio" | bc 2>/dev/null || python3 -I -c "print($fin-$inicio)")
echo
printf '%d casos en %.1f s (límite RQ-02: 30 s, mínimo 10 casos)\n' "$casos" "$dur"
if [ "$fallos" -eq 0 ]; then echo "TODO EN VERDE"; else echo "$fallos FALLAS"; exit 1; fi
