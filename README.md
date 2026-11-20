# Núcleo de huella digital — Silicon Minds (Arquitectura de Computadores)

SHA-256 escrito en C (`libhuella.so`), con el estado en C entre llamadas para procesar archivos de cualquier tamaño por bloques de 64 KB, integrado en `huella-service` (Spring Boot + JNA) y desplegable con Docker.

```
native/           biblioteca en C, suite de pruebas y Makefile        (entregables 01 y 03)
huella-service/   interfaz JNA, @Service, API y banco de medición     (entregable 02)
Dockerfile        multietapa: gcc → Maven → JRE sin compilador        (entregable 05)
docker-compose.yml  manifiesto con el límite de 128 MB declarado      (entregable 05)
bench/            scripts de medición y datos crudos                  (entregable 04)
BITACORA.md       decisiones, alternativas y requisito que las justifica (entregable 06)
```

## Comandos

| Para… | Comando | Necesita |
|---|---|---|
| Compilar y probar la biblioteca (RQ-02) | `make -C native test` | gcc, make (valgrind opcional) |
| Levantar todo desde cero (RQ-03) | `docker compose up --build` | solo Docker |
| Probar la API | `curl --data-binary @archivo -H 'Content-Type: application/octet-stream' 'localhost:8080/api/huellas?bits=16'` | servicio levantado |
| Banco C contra Java (RQ-01) | `bench/banco.sh` · en contenedor: `bench/banco_contenedor.sh` | JDK 21 + Maven · Docker |
| Medir despliegue (RQ-03) | `bench/despliegue.sh` | Docker, `curl`, `bc` |
| Pruebas Java | `make -C native && mvn -f huella-service test` | JDK 21 + Maven |

## Qué se comprobó en esta entrega

**Suite nativa (RQ-02) — ejecutada, en verde: 32 casos en ~1 s** (límite: 30 s y mínimo 10 casos).

| Requisito | Resultado |
|---|---|
| RN-01 vectores del estándar | `""`, `"abc"` y el vector de 448 bits coinciden |
| RN-02 bloques equivalentes | 100 003 bytes: 64 KB = 1024 B = 7 B = una pasada; y contra `sha256sum` |
| RN-03 relleno | 55, 56 y 64 bytes coinciden con `sha256sum` |
| RN-04 avalancha | 20 pares de 1 bit: media **50,1 %** (mín. 47,3 %, máx. 53,5 %) — ver D-10 |
| RN-05 reducida | prefijo de 16 y 24 bits correcto; colisión de 16 bits tras **394 intentos (0,0001 s)** |
| RN-06 fugas | 10 000 ciclos: memoria residente +60 KB (ruido; una fuga serían ~1,2 MB); `valgrind --leak-check=full` sin errores en 2 000 ciclos |
| `-Wall` | compila con `-Wall -Wextra -Werror` sin advertencias |

**Java (JNA, `@Service`)** — `HuellaService`, `LibHuella`, `HuellaController` y `BancoMedicion` compilan con JDK 21, y los **9 casos de `HuellaServiceTest` pasan** ejecutados contra `libhuella.so` con un sustituto de JNA (API FFM de Java 21) y sustitutos mínimos de JUnit y de las anotaciones de Spring, porque Maven Central no era accesible. Entre ellos: coincidencia con `MessageDigest` en 14 tamaños (bordes del relleno incluidos), lecturas cortas de `read()`, `huella_liberar` llamado en el `finally` aunque la lectura falle, 10 000 ciclos sin fuga, y `NULL` de `huella_nueva` traducido a excepción. Con Spring y JNA reales todavía no se ha corrido.

## RQ-01: C contra Java — resultado negativo, reportado y explicado

Medición local (no en el contenedor), archivo de 50 MB, bloques de 64 KB, 3 corridas descartadas + 5 medidas, mediana:

| | MB/s |
|---|---|
| `MessageDigest` (Java) | **1105,9** |
| `libhuella.so` desde Java (frontera por bloque de 64 KB) | 199,1 |
| `libhuella.so` desde C puro, sin Java | 223,1 |

**C no le gana a Java: es 5–6 veces más lento.** Las huellas son idénticas, así que el cálculo es correcto; lo que pasa es otra cosa:

1. `MessageDigest` y `sha256sum` usan las instrucciones SHA del procesador (SHA-NI); la prueba se hizo en un CPU que las tiene. El C de esta entrega es el ciclo de compresión portable (rotaciones y sumas módulo 2³² sobre `uint32_t`), que es lo que pide el curso y corre en cualquier procesador (D-07).
2. La frontera Java↔C no es la culpable: costó cerca de un 10 % (199 contra 223 MB/s), porque se cruza 800 veces para 50 MB y no 52 millones (D-02).
3. Si el objetivo fuera ganar velocidad, la vía sería SHA-NI con detección en tiempo de ejecución. Está descartada por alcance y portabilidad, no por imposible.

Datos crudos en `bench/resultados_locales/RQ-01_sin_contenedor.txt`.

## Qué NO se pudo medir aquí (hay que correrlo en tu máquina)

El entorno donde construí esto no tenía daemon de Docker ni acceso a Maven Central, así que:

- **RQ-01 en contenedor de 128 MB** (memoria plana con `docker stats`, respuesta < 2 s) → `bench/banco_contenedor.sh`. Los parámetros de JVM del `Dockerfile` para caber en 128 MB son una hipótesis sin confirmar (D-09).
- **RQ-03** (imagen < 250 MB sin `gcc`, primera huella correcta < 90 s) → `bench/despliegue.sh`. El `Dockerfile` y el manifiesto no se construyeron; solo se validó el manifiesto con `docker compose config` (128 MiB = 134 217 728 B) y el comando del healthcheck contra un servidor local.
- **El proyecto Maven** (`huella-service`) no se construyó con Spring ni JNA reales (el arranque de Spring, el endpoint HTTP y el `pom.xml` están sin ejecutar). Lo primero que debes correr: `make -C native && mvn -f huella-service test`.
- **La cifra de C-vía-JNA** (199 MB/s) salió del sustituto FFM; con JNA real puede variar. Repetirla con `bench/banco.sh`.

## Fuera de alcance (según el documento)

Frontend Angular, autenticación y autorización, registro e historial de archivos, firma digital y cifrado. El servicio no toca la configuración de seguridad común: el `HuellaController` queda protegido igual que los demás una vez integrado al sistema. Los demás servicios del frente de Web 2 se agregan al mismo `docker-compose.yml`.

## Contrato congelado con el frente de Web 2

Está en `native/huella.h` y `BITACORA.md` (D-03 y D-04): quién libera la memoria, cómo se reportan los errores, y los casos de referencia. Falta que el frente de Web 2 lo firme en la bitácora común.
