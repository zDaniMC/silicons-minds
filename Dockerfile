# syntax=docker/dockerfile:1
#
# Multietapa (RQ-03). La imagen final NO lleva compilador ni fuentes en C: solo libhuella.so y el JAR.
# Las etapas de construcción y la imagen final usan la misma base glibc (Ubuntu 22.04 "jammy"):
# una .so compilada contra glibc no carga en Alpine (musl). Ver BITACORA.md, D-08.

# ---------- Etapa 1: compila libhuella.so con gcc ----------
FROM ubuntu:22.04 AS nativo
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc make libc6-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY native/huella.h native/huella.c native/Makefile ./
RUN make libhuella.so          # gcc -O2 -Wall -Wextra -Werror -shared -fPIC

# ---------- Etapa 2: construye el JAR de huella-service ----------
FROM maven:3.9-eclipse-temurin-21-jammy AS java
WORKDIR /build
COPY huella-service/pom.xml .
RUN mvn -q -B dependency:go-offline
COPY huella-service/src ./src
# Las pruebas que necesitan la .so corren en la suite nativa (make -C native test) y en CI;
# aquí solo se empaqueta.
RUN mvn -q -B -DskipTests package

# ---------- Etapa 3: imagen final (sin gcc, sin fuentes) ----------
FROM eclipse-temurin:21-jre-jammy
RUN useradd --system --uid 10001 --no-create-home huella
COPY --from=nativo /src/libhuella.so /opt/huella/lib/libhuella.so
COPY --from=java   /build/target/huella-service-1.0.0.jar /app/huella-service.jar

# D-06: JNA busca libhuella.so en jna.library.path. Si no está, el servicio falla al arrancar
# (UnsatisfiedLinkError con las rutas probadas), no a mitad de una petición.
# Parámetros de JVM para caber en 128 MB: heap acotado, GC serial, metaspace y code cache limitados.
ENV JAVA_TOOL_OPTIONS="-Djna.library.path=/opt/huella/lib -XX:MaxRAMPercentage=50 -XX:+UseSerialGC -XX:MaxMetaspaceSize=80m -XX:ReservedCodeCacheSize=32m -Xss512k -Dstdout.encoding=UTF-8"

USER 10001
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "/app/huella-service.jar"]
