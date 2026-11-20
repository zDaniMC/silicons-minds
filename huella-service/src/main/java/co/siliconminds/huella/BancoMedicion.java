package co.siliconminds.huella;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Banco de medición de RQ-01: el mismo archivo con libhuella.so (vía JNA) y con
 * MessageDigest.getInstance("SHA-256"). Descarta las 3 primeras corridas de cada lado
 * (calentamiento del JIT), mide 5 y reporta la mediana en MB/s. Verifica que las dos huellas
 * sean idénticas.
 *
 *   java -Djna.library.path=native -cp huella-service.jar co.siliconminds.huella.BancoMedicion [archivo] [MB]
 *
 * Sin archivo, genera uno de 50 MB (o de [MB]) en el directorio temporal.
 * Imprime una línea CSV por corrida (datos crudos) y un resumen.
 */
public final class BancoMedicion {

    static final int DESCARTADAS = 3;
    static final int MEDIDAS = 5;

    private BancoMedicion() {}

    public static void main(String[] args) throws Exception {
        Path archivo;
        boolean temporal = false;
        if (args.length > 0 && !args[0].isBlank()) {
            archivo = Path.of(args[0]);
        } else {
            int mb = args.length > 1 ? Integer.parseInt(args[1]) : 50;
            archivo = generar(mb);
            temporal = true;
        }
        try {
            ejecutar(archivo);
        } finally {
            if (temporal) Files.deleteIfExists(archivo);
        }
    }

    static void ejecutar(Path archivo) throws IOException, NoSuchAlgorithmException {
        long bytes = Files.size(archivo);
        double mb = bytes / 1048576.0;
        HuellaService nativa = new HuellaService();

        List<Double> velNativa = new ArrayList<>();
        List<Double> velJava = new ArrayList<>();
        String huellaNativa = null, huellaJava = null;

        System.out.printf("archivo=%s tamano=%.1f MB bloque=%d bytes%n", archivo, mb, HuellaService.TAM_BLOQUE);
        System.out.println("lado,corrida,descartada,segundos,MB_por_s");

        for (int i = 1; i <= DESCARTADAS + MEDIDAS; i++) {
            boolean descartada = i <= DESCARTADAS;
            long t0 = System.nanoTime();
            try (InputStream in = new FileInputStream(archivo.toFile())) {
                huellaNativa = HuellaService.hex(nativa.huella(in));
            }
            double s = (System.nanoTime() - t0) / 1e9;
            if (!descartada) velNativa.add(mb / s);
            System.out.printf("C-JNA,%d,%s,%.4f,%.1f%n", i, descartada, s, mb / s);
        }
        for (int i = 1; i <= DESCARTADAS + MEDIDAS; i++) {
            boolean descartada = i <= DESCARTADAS;
            long t0 = System.nanoTime();
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            try (InputStream in = new FileInputStream(archivo.toFile())) {
                byte[] buf = new byte[HuellaService.TAM_BLOQUE];
                int n;
                while ((n = in.read(buf)) > 0) md.update(buf, 0, n);
            }
            huellaJava = HuellaService.hex(md.digest());
            double s = (System.nanoTime() - t0) / 1e9;
            if (!descartada) velJava.add(mb / s);
            System.out.printf("MessageDigest,%d,%s,%.4f,%.1f%n", i, descartada, s, mb / s);
        }

        double mC = mediana(velNativa), mJ = mediana(velJava);
        System.out.println();
        System.out.println("huella C   = " + huellaNativa);
        System.out.println("huella Java= " + huellaJava);
        System.out.println(huellaNativa.equals(huellaJava) ? "las dos huellas son idénticas"
                                                          : "ERROR: las huellas DIFIEREN");
        System.out.printf("mediana C vía JNA : %.1f MB/s%n", mC);
        System.out.printf("mediana MessageDigest: %.1f MB/s%n", mJ);
        System.out.printf("C / Java = %.2f  (%s)%n", mC / mJ, mC >= mJ ? "C es más rápido" : "C NO es más rápido: reportar y explicar");
        if (!huellaNativa.equals(huellaJava)) System.exit(1);
    }

    static double mediana(List<Double> v) {
        double[] a = v.stream().mapToDouble(Double::doubleValue).sorted().toArray();
        return a[a.length / 2];
    }

    static Path generar(int mb) throws IOException {
        Path p = Files.createTempFile("banco-", ".bin");
        byte[] bloque = new byte[1 << 20];
        Random r = new Random(1);
        try (var out = Files.newOutputStream(p)) {
            for (int i = 0; i < mb; i++) {
                r.nextBytes(bloque);
                out.write(bloque);
            }
        }
        return p;
    }
}
