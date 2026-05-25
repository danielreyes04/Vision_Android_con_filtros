package com.example.parcial;

import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.ingenieriiajhr.jhrCameraX.BitmapResponse;
import com.ingenieriiajhr.jhrCameraX.CameraJhr;
import com.google.common.util.concurrent.ListenableFuture;

import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.imgproc.Imgproc;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("parcial");
    }

    // ── Funciones C++ existentes ──────────────────────────────
    public native void procesarFiltroC(long addrInput, int tipoFiltro);

    // Calibrar monedas desde assets (Lógica de SebastianUrrego)
    public native void calibrateCoins(android.content.res.AssetManager assetManager);

    // ── Funciones C++ NUEVAS ──────────────────────────────────
    // Punto 1: Detección de Rostros (Viola-Jones / Haar Cascade)
    public native void detectarRostrosC(long addrInput, String rutaCascade);

    // Punto 2: Detección de Monedas Colombianas (HoughCircles)
    //          Retorna el total en pesos detectado
    public native int detectarMonedasC(long addrInput);

    private static final String TAG = "MainActivity";

    CameraJhr cameraJhr;
    ImageView imgBitmap;
    PreviewView previewImg;

    // ── Botones existentes ────────────────────────────────────
    Button btnNormal, btnFiltro, btnGaleria;

    // ── Botones NUEVOS ────────────────────────────────────────
    Button btnRostros, btnMonedas, btnGirar;

    int filtroActual = 0;
    int camaraActual = 0; // 0 para trasera, 1 para frontal
    final String[] nombresFiltro = {"Normal", "Gris", "Canny", "Negativo", "Verde→Rojo"};

    // Modo actual: "filtros" | "rostros" | "monedas"
    String modoActual = "filtros";

    Bitmap bitmapGaleria = null;
    private ActivityResultLauncher<Intent> galeriaLauncher;

    // Ruta local del archivo cascade (se copia desde assets una sola vez)
    private String rutaCascadeLocal = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_main);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        if (!OpenCVLoader.initLocal()) {
            Log.e(TAG, "No se pudo inicializar OpenCV");
        }

        // Calibrar monedas usando las imágenes de assets
        calibrateCoins(getAssets());

        // Copiar el cascade XML desde assets al almacenamiento interno
        // (el C++ necesita una ruta de archivo real, no un asset stream)
        rutaCascadeLocal = copiarCascadeDeAssets();

        previewImg = findViewById(R.id.previewImg);
        imgBitmap  = findViewById(R.id.imagBitmap);
        btnNormal  = findViewById(R.id.btnNormal);
        btnFiltro  = findViewById(R.id.btnBordes);
        btnGaleria = findViewById(R.id.btnGaleria);

        // ── IDs nuevos: agrégalos también en activity_main.xml ──
        btnRostros = findViewById(R.id.btnRostros);
        btnMonedas = findViewById(R.id.btnMonedas);
        btnGirar   = findViewById(R.id.btnGirar);

        imgBitmap.setVisibility(View.GONE);

        galeriaLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                        Uri uri = result.getData().getData();
                        try {
                            InputStream stream = getContentResolver().openInputStream(uri);
                            bitmapGaleria = BitmapFactory.decodeStream(stream);
                            stream.close();

                            toggleCameraPreview(false);

                            // Aplicar el modo que esté activo
                            if (modoActual.equals("rostros")) {
                                aplicarRostrosAGaleria();
                            } else if (modoActual.equals("monedas")) {
                                aplicarMonedasAGaleria();
                            } else {
                                if (filtroActual == 0) {
                                    filtroActual = 1;
                                    btnFiltro.setText("Filtro: " + nombresFiltro[filtroActual]);
                                }
                                aplicarFiltroAGaleria();
                            }
                        } catch (Exception e) {
                            Log.e(TAG, "Error al cargar imagen: " + e.getMessage());
                        }
                    }
                }
        );

        // ── Listeners existentes ──────────────────────────────

        btnNormal.setOnClickListener(v -> {
            modoActual = "filtros";
            filtroActual = 0;
            bitmapGaleria = null;
            toggleCameraPreview(true);
            imgBitmap.setVisibility(View.GONE);
            btnFiltro.setText("Filtro: Gris");
        });

        btnFiltro.setOnClickListener(v -> {
            modoActual = "filtros";
            if (filtroActual == 0) {
                filtroActual = 1;
            } else {
                filtroActual = (filtroActual % 4) + 1;
            }
            imgBitmap.setVisibility(View.VISIBLE);
            btnFiltro.setText("Filtro: " + nombresFiltro[filtroActual]);

            if (bitmapGaleria != null) {
                aplicarFiltroAGaleria();
            }
        });

        btnGaleria.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_PICK);
            intent.setType("image/*");
            galeriaLauncher.launch(intent);
        });

        // ── Listeners NUEVOS ──────────────────────────────────

        btnRostros.setOnClickListener(v -> {
            modoActual = "rostros";
            bitmapGaleria = null;
            // Activar cámara frontal (parámetro lensFacing = 1 en CameraJhr)
            toggleCameraPreview(true);
            imgBitmap.setVisibility(View.VISIBLE);
            Toast.makeText(this, "Detección de rostros activada", Toast.LENGTH_SHORT).show();
        });

        btnMonedas.setOnClickListener(v -> {
            modoActual = "monedas";
            bitmapGaleria = null;
            toggleCameraPreview(true);
            imgBitmap.setVisibility(View.VISIBLE);
            Toast.makeText(this, "Detección de monedas activada", Toast.LENGTH_SHORT).show();
        });

        btnGirar.setOnClickListener(v -> {
            camaraActual = (camaraActual == 0) ? 1 : 0;

            // 1. Obtener el proveedor de la cámara de AndroidX para limpiar el ciclo de vida
            ListenableFuture<ProcessCameraProvider> cameraProviderFuture =
                    ProcessCameraProvider.getInstance(this);

            cameraProviderFuture.addListener(() -> {
                try {
                    // 2. Forzar la desvinculación de TODAS las cámaras activas
                    ProcessCameraProvider cameraProvider = cameraProviderFuture.get();
                    cameraProvider.unbindAll();

                    // 3. Reiniciar la librería desde cero con el nuevo ID de cámara
                    cameraJhr = new CameraJhr(this);
                    startCameraJhr();

                } catch (Exception e) {
                    Log.e(TAG, "Error al girar: " + e.getMessage());
                }
            }, ContextCompat.getMainExecutor(this));
        });

        cameraJhr = new CameraJhr(this);
    }

    // ── Copiar cascade desde assets ───────────────────────────
    // Descarga el XML de:
    // https://github.com/opencv/opencv/raw/master/data/haarcascades/haarcascade_frontalface_default.xml
    // y ponlo en: app/src/main/assets/haarcascade_frontalface_default.xml
    private String copiarCascadeDeAssets() {
        File destino = new File(getFilesDir(), "haarcascade_frontalface_default.xml");
        if (destino.exists()) return destino.getAbsolutePath(); // ya fue copiado antes

        try {
            InputStream is = getAssets().open("haarcascade_frontalface_default.xml");
            FileOutputStream fos = new FileOutputStream(destino);
            byte[] buffer = new byte[4096];
            int leidos;
            while ((leidos = is.read(buffer)) != -1) fos.write(buffer, 0, leidos);
            is.close();
            fos.close();
            Log.d(TAG, "Cascade copiado a: " + destino.getAbsolutePath());
        } catch (Exception e) {
            Log.e(TAG, "Error copiando cascade: " + e.getMessage());
            return null;
        }
        return destino.getAbsolutePath();
    }

    // ── Helpers de procesamiento ──────────────────────────────

    private void toggleCameraPreview(boolean mostrar) {
        previewImg.setVisibility(mostrar ? View.VISIBLE : View.INVISIBLE);
    }

    /** Procesa un Bitmap con el filtro C++ seleccionado (lógica existente) */
    private Bitmap procesarConFiltro(Bitmap bitmap) {
        Bitmap bmp = bitmap.copy(Bitmap.Config.ARGB_8888, true);
        Mat mat = new Mat(bmp.getHeight(), bmp.getWidth(), CvType.CV_8UC4);
        Utils.bitmapToMat(bmp, mat);

        procesarFiltroC(mat.getNativeObjAddr(), filtroActual);

        Mat matSalida;
        if (mat.channels() == 1) {
            matSalida = new Mat();
            Imgproc.cvtColor(mat, matSalida, Imgproc.COLOR_GRAY2RGBA);
            mat.release();
        } else {
            matSalida = mat;
        }

        Bitmap resultado = Bitmap.createBitmap(matSalida.cols(), matSalida.rows(), Bitmap.Config.ARGB_8888);
        Utils.matToBitmap(matSalida, resultado);
        matSalida.release();
        return resultado;
    }

    /** Procesa un Bitmap con detección de rostros C++ */
    private Bitmap procesarConRostros(Bitmap bitmap) {
        if (rutaCascadeLocal == null) {
            Toast.makeText(this, "Cascade no disponible", Toast.LENGTH_SHORT).show();
            return bitmap;
        }
        Bitmap bmp = bitmap.copy(Bitmap.Config.ARGB_8888, true);
        Mat mat = new Mat(bmp.getHeight(), bmp.getWidth(), CvType.CV_8UC4);
        Utils.bitmapToMat(bmp, mat);

        // Llamada al C++ nuevo
        detectarRostrosC(mat.getNativeObjAddr(), rutaCascadeLocal);

        Bitmap resultado = Bitmap.createBitmap(mat.cols(), mat.rows(), Bitmap.Config.ARGB_8888);
        Utils.matToBitmap(mat, resultado);
        mat.release();
        return resultado;
    }

    /** Procesa un Bitmap con detección de monedas C++ */
    private Bitmap procesarConMonedas(Bitmap bitmap) {
        Bitmap bmp = bitmap.copy(Bitmap.Config.ARGB_8888, true);
        Mat mat = new Mat(bmp.getHeight(), bmp.getWidth(), CvType.CV_8UC4);
        Utils.bitmapToMat(bmp, mat);

        // Llamada al C++ nuevo (retorna el total en pesos)
        int total = detectarMonedasC(mat.getNativeObjAddr());
        Log.d(TAG, "Total monedas detectado: $" + total + " COP");

        Bitmap resultado = Bitmap.createBitmap(mat.cols(), mat.rows(), Bitmap.Config.ARGB_8888);
        Utils.matToBitmap(mat, resultado);
        mat.release();
        return resultado;
    }

    private void aplicarFiltroAGaleria() {
        if (bitmapGaleria == null) return;
        if (filtroActual == 0) {
            imgBitmap.setVisibility(View.VISIBLE);
            imgBitmap.setImageBitmap(bitmapGaleria);
            return;
        }
        Bitmap resultado = procesarConFiltro(bitmapGaleria);
        imgBitmap.setVisibility(View.VISIBLE);
        imgBitmap.setImageBitmap(resultado);
    }

    private void aplicarRostrosAGaleria() {
        if (bitmapGaleria == null) return;
        Bitmap resultado = procesarConRostros(bitmapGaleria);
        imgBitmap.setVisibility(View.VISIBLE);
        imgBitmap.setImageBitmap(resultado);
    }

    private void aplicarMonedasAGaleria() {
        if (bitmapGaleria == null) return;
        Bitmap resultado = procesarConMonedas(bitmapGaleria);
        imgBitmap.setVisibility(View.VISIBLE);
        imgBitmap.setImageBitmap(resultado);
    }

    // ── Cámara ───────────────────────────────────────────────

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && cameraJhr.allpermissionsGranted() && !cameraJhr.getIfStartCamera()) {
            startCameraJhr();
        } else if (!cameraJhr.allpermissionsGranted()) {
            cameraJhr.noPermissions();
        }
    }

    private void startCameraJhr() {
        // Solo configuramos el listener e inicializamos si la cámara no está ya corriendo
        // o si necesitamos reconfigurarla debido a un cambio de lente.
        cameraJhr.addlistenerBitmap(new BitmapResponse() {
            @Override
            public void bitmapReturn(@Nullable Bitmap bitmap) {
                if (bitmap == null || bitmapGaleria != null) return;

                Bitmap bmpFinal = null;

                if (modoActual.equals("rostros")) {
                    bmpFinal = procesarConRostros(bitmap);
                } else if (modoActual.equals("monedas")) {
                    bmpFinal = procesarConMonedas(bitmap);
                } else if (filtroActual != 0) {
                    bmpFinal = procesarConFiltro(bitmap);
                }

                if (bmpFinal != null) {
                    final Bitmap finalBmp = bmpFinal;
                    runOnUiThread(() -> {
                        imgBitmap.setVisibility(View.VISIBLE);
                        imgBitmap.setImageBitmap(finalBmp);
                    });
                }
            }
        });

        cameraJhr.initBitmap();
        // El primer parámetro es el selector de cámara (0 trasera, 1 frontal)
        cameraJhr.start(camaraActual, 0, previewImg, true, false, true);
    }
}