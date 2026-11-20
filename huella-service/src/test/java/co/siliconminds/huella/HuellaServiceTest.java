package co.siliconminds.huella;

import com.sun.jna.Pointer;
import org.junit.jupiter.api.Test;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;

import static org.junit.jupiter.api.Assertions.*;

/** Línea base: MessageDigest de Java es el oráculo del lado de Java (el de la terminal es sha256sum). */
class HuellaServiceTest {

    private final HuellaService servicio = new HuellaService();

    private static byte[] sha256Java(byte[] datos) throws NoSuchAlgorithmException {
        return MessageDigest.getInstance("SHA-256").digest(datos);
    }

    @Test
    void vectoresDelEstandar() {
        assertEquals("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                HuellaService.hex(servicio.huella(new byte[0])));
        assertEquals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                HuellaService.hex(servicio.huella("abc".getBytes(StandardCharsets.US_ASCII))));
    }

    @Test
    void coincideConMessageDigestEnLosBordesDelRelleno() throws Exception {
        Random r = new Random(7);
        for (int n : new int[]{0, 1, 55, 56, 63, 64, 65, 119, 120, 65535, 65536, 65537, 100_003, 1_000_000}) {
            byte[] datos = new byte[n];
            r.nextBytes(datos);
            assertArrayEquals(sha256Java(datos), servicio.huella(datos), "tamaño " + n);
        }
    }

    /** read() puede devolver menos bytes que el buffer: lo que se pasa a C es lo realmente leído. */
    @Test
    void lecturasCortasDanLaMismaHuella() throws Exception {
        byte[] datos = new byte[100_003];
        new Random(3).nextBytes(datos);
        InputStream goteo = new InputStream() {
            private final ByteArrayInputStream in = new ByteArrayInputStream(datos);
            @Override public int read() { return in.read(); }
            @Override public int read(byte[] b, int off, int len) { return in.read(b, off, Math.min(len, 777)); }
        };
        assertArrayEquals(sha256Java(datos), servicio.huella(goteo));
    }

    @Test
    void huellaReducidaEsElPrefijoDeLaCompleta() {
        byte[] completa = servicio.huella("abc".getBytes(StandardCharsets.US_ASCII));
        byte[] r16 = servicio.huellaReducida(completa, 16);
        byte[] r24 = servicio.huellaReducida(completa, 24);
        assertEquals(2, r16.length);
        assertEquals(3, r24.length);
        assertArrayEquals(java.util.Arrays.copyOf(completa, 2), r16);
        assertArrayEquals(java.util.Arrays.copyOf(completa, 3), r24);
    }

    @Test
    void argumentosInvalidosSeTraducenAExcepcion() {
        byte[] completa = servicio.huella(new byte[0]);
        assertEquals(HuellaNativaException.Causa.ARGUMENTO_INVALIDO,
                assertThrows(HuellaNativaException.class, () -> servicio.huellaReducida(completa, 0)).causa());
        assertThrows(HuellaNativaException.class, () -> servicio.huellaReducida(completa, 257));
        assertThrows(HuellaNativaException.class, () -> servicio.huellaReducida(new byte[5], 16));
    }

    @Test
    void avalanchaCercaDeLaMitad() {
        byte[] a = servicio.huella("pago: 100".getBytes(StandardCharsets.US_ASCII));
        byte[] b = servicio.huella("pago: 101".getBytes(StandardCharsets.US_ASCII));
        assertEquals(129, HuellaService.bitsDistintos(a, b));
    }

    // ---- RN-06 / contrato: quien reserva, libera, incluso si algo falla ----

    /** Biblioteca simulada que cuenta reservas y liberaciones; no necesita el .so. */
    private static class LibContadora implements LibHuella {
        final AtomicInteger nuevas = new AtomicInteger(), liberadas = new AtomicInteger();
        private final Pointer estado = new Pointer(1);
        public Pointer huella_nueva() { nuevas.incrementAndGet(); return estado; }
        public void huella_actualizar(Pointer e, byte[] b, int n) { }
        public void huella_final(Pointer e, byte[] s) { }
        public void huella_liberar(Pointer e) { liberadas.incrementAndGet(); }
        public void huella_reducida(byte[] e, int n, byte[] s) { }
    }

    @Test
    void liberaElEstadoAunqueLaLecturaFalle() {
        LibContadora lib = new LibContadora();
        HuellaService s = new HuellaService(lib);
        InputStream roto = new InputStream() {
            @Override public int read() throws IOException { throw new IOException("disco"); }
        };
        assertThrows(IOException.class, () -> s.huella(roto));
        assertEquals(1, lib.nuevas.get());
        assertEquals(1, lib.liberadas.get(), "huella_liberar debe llamarse en el finally");
    }

    @Test
    void diezMilCiclosLiberanTodo() {
        LibContadora lib = new LibContadora();
        HuellaService s = new HuellaService(lib);
        for (int i = 0; i < 10_000; i++) s.huella(new byte[100]);
        assertEquals(10_000, lib.nuevas.get());
        assertEquals(10_000, lib.liberadas.get());
    }

    @Test
    void estadoNuloSeTraduceAExcepcion() {
        LibHuella sinMemoria = new LibContadora() {
            @Override public Pointer huella_nueva() { return null; }
        };
        var e = assertThrows(HuellaNativaException.class, () -> new HuellaService(sinMemoria).huella(new byte[1]));
        assertEquals(HuellaNativaException.Causa.SIN_MEMORIA, e.causa());
    }
}
