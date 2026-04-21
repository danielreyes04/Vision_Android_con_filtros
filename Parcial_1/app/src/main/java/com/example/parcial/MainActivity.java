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

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.view.PreviewView;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.ingenieriiajhr.jhrCameraX.BitmapResponse;
import com.ingenieriiajhr.jhrCameraX.CameraJhr;

import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.imgproc.Imgproc;

import java.io.InputStream;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("parcial");
    }

    public native void procesarFiltroC(long addrInput, int tipoFiltro);

    private static final String TAG = "MainActivity";

    CameraJhr cameraJhr;
    ImageView imgBitmap;
    PreviewView previewImg;
    Button btnNormal, btnFiltro, btnGaleria;

    int filtroActual = 0;
    final String[] nombresFiltro = {"Normal", "Gris", "Canny", "Negativo", "Verde→Rojo"};

    Bitmap bitmapGaleria = null;
    private ActivityResultLauncher<Intent> galeriaLauncher;

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

        previewImg = findViewById(R.id.previewImg);
        imgBitmap  = findViewById(R.id.imagBitmap);
        btnNormal  = findViewById(R.id.btnNormal);
        btnFiltro  = findViewById(R.id.btnBordes);
        btnGaleria = findViewById(R.id.btnGaleria);

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

                            if (filtroActual == 0) {
                                filtroActual = 1;
                                btnFiltro.setText("Filtro: " + nombresFiltro[filtroActual]);
                            }

                            aplicarFiltroAGaleria();

                        } catch (Exception e) {
                            Log.e(TAG, "Error al cargar imagen: " + e.getMessage());
                        }
                    }
                }
        );

        btnNormal.setOnClickListener(v -> {
            filtroActual = 0;
            bitmapGaleria = null;
            toggleCameraPreview(true);
            imgBitmap.setVisibility(View.GONE);
            btnFiltro.setText("Filtro: Gris");
        });

        btnFiltro.setOnClickListener(v -> {
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

        cameraJhr = new CameraJhr(this);
    }

    private void toggleCameraPreview(boolean mostrar) {
        previewImg.setVisibility(mostrar ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Convierte Bitmap a Mat RGBA y lo pasa directamente al C++
     * que ya espera RGBA internamente.
     */
    private Bitmap procesarConFiltro(Bitmap bitmap) {
        Bitmap bmp = bitmap.copy(Bitmap.Config.ARGB_8888, true);

        // Bitmap → Mat RGBA (el C++ ya espera RGBA)
        Mat mat = new Mat(bmp.getHeight(), bmp.getWidth(), CvType.CV_8UC4);
        Utils.bitmapToMat(bmp, mat);

        // Procesar con C++
        procesarFiltroC(mat.getNativeObjAddr(), filtroActual);

        // Si el resultado quedó en 1 canal (Gris/Canny) convertir a RGBA
        Mat matSalida;
        if (mat.channels() == 1) {
            matSalida = new Mat();
            Imgproc.cvtColor(mat, matSalida, Imgproc.COLOR_GRAY2RGBA);
            mat.release();
        } else {
            matSalida = mat;
        }

        // Mat → Bitmap
        Bitmap resultado = Bitmap.createBitmap(matSalida.cols(), matSalida.rows(), Bitmap.Config.ARGB_8888);
        Utils.matToBitmap(matSalida, resultado);
        matSalida.release();

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
        cameraJhr.addlistenerBitmap(new BitmapResponse() {
            @Override
            public void bitmapReturn(@Nullable Bitmap bitmap) {
                if (bitmap == null) return;
                if (bitmapGaleria != null) return;
                if (filtroActual == 0) return;

                final Bitmap bmpFinal = procesarConFiltro(bitmap);
                runOnUiThread(() -> imgBitmap.setImageBitmap(bmpFinal));
            }
        });

        cameraJhr.initBitmap();
        cameraJhr.start(0, 0, previewImg, true, false, true);
    }
}