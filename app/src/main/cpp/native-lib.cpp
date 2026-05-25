#include <jni.h>
#include <opencv2/opencv.hpp>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace cv;
using namespace std;

#define LOG_TAG "FiltrosC++"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

struct RefCoin {
    long value;
    int minRadius;
    int maxRadius;
};

static vector<RefCoin> g_calibratedCoins;

extern "C" JNIEXPORT void JNICALL
Java_com_example_parcial_MainActivity_calibrateCoins(
        JNIEnv *env, jobject, jobject assetManager) {
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    if (!mgr) return;

    struct AssetDef {
        long val;
        string file;
    };

    vector<AssetDef> assets = {
            {1000, "1000_frente.jpeg"}, {1000, "1000_trasera.jpeg"},
            {500,  "500_nueva_frente.jpeg"}, {500, "500_nueva_trasera.jpeg"},
            {500,  "500_vieja_frente.jpeg"}, {500, "500_vieja_trasera.jpeg"},
            {200,  "200_nueva_frente.jpeg"}, {200, "200_nueva_trasera.jpeg"},
            {200,  "200_vieja_frente.jpeg"}, {200, "200_vieja_trasera.jpeg"},
            {100,  "100_nueva_frente.jpeg"}, {100, "100_nueva_trasera.jpeg"},
            {100,  "100_vieja_frente.jpeg"}, {100, "100_vieja_trasera.jpeg"},
            {50,   "50_nueva_frente.jpeg"},  {50,  "50_nueva_trasera.jpeg"}};

    g_calibratedCoins.clear();

    for (const auto &ad : assets) {
        AAsset *asset = AAssetManager_open(mgr, ad.file.c_str(), AASSET_MODE_BUFFER);
        if (!asset) continue;

        off_t length = AAsset_getLength(asset);
        const void *data = AAsset_getBuffer(asset);
        if (!data) {
            AAsset_close(asset);
            continue;
        }

        vector<uchar> buffer((const char *)data, (const char *)data + length);
        AAsset_close(asset);

        Mat img = imdecode(buffer, IMREAD_COLOR);
        if (img.empty()) continue;

        Mat gray, blurred;
        cvtColor(img, gray, COLOR_BGR2GRAY);
        GaussianBlur(gray, blurred, Size(9, 9), 2.0);
        medianBlur(blurred, blurred, 5);

        int minR = (int)(img.cols * 0.05f);
        int maxR = (int)(img.cols * 0.09f);

        vector<Vec3f> circles;
        HoughCircles(blurred, circles, HOUGH_GRADIENT, 1.5, img.cols * 0.06f, 100, 60, minR, maxR);

        if (!circles.empty()) {
            int r = cvRound(circles[0][2]);
            RefCoin rc;
            rc.value = ad.val;
            rc.minRadius = r - 15;
            rc.maxRadius = r + 15;
            g_calibratedCoins.push_back(rc);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_parcial_MainActivity_procesarFiltroC(JNIEnv *env, jobject thiz, jlong addrInput, jint tipoFiltro) {

    Mat &img = *(Mat *) addrInput;

    if (img.empty()) {
        LOGD("Error: La imagen recibida está vacía");
        return;
    }

    if (tipoFiltro == 1) {
        // --- FILTRO ESCALA DE GRISES ---
        cvtColor(img, img, COLOR_RGBA2GRAY);
        cvtColor(img, img, COLOR_GRAY2RGBA);
        LOGD("Filtro Gris aplicado");
    }
    else if (tipoFiltro == 2) {
        // --- FILTRO CANNY (BORDES) ---
        Mat gray, edges;
        cvtColor(img, gray, COLOR_RGBA2GRAY);
        Canny(gray, edges, 50, 150);
        cvtColor(edges, img, COLOR_GRAY2RGBA);
        LOGD("Filtro Canny aplicado");
    }
    else if (tipoFiltro == 3) {
        // --- FILTRO NEGATIVO ---
        Mat canales[4];
        split(img, canales);
        bitwise_not(canales[0], canales[0]);
        bitwise_not(canales[1], canales[1]);
        bitwise_not(canales[2], canales[2]);
        merge(canales, 4, img);
        LOGD("Filtro Negativo aplicado");
    }
    else if (tipoFiltro == 4) {
        // Verificar que la imagen tenga 4 canales antes de procesar
        if (img.channels() != 4) {
            LOGD("Error: imagen no tiene 4 canales, tiene %d", img.channels());
            return;
        }

        Mat bgr, hsv, greenMask;

        // RGBA → BGR
        cvtColor(img, bgr, COLOR_RGBA2BGR);

        // BGR → HSV
        cvtColor(bgr, hsv, COLOR_BGR2HSV);

        // Máscara de verdes
        inRange(hsv, Scalar(40, 50, 50), Scalar(80, 255, 255), greenMask);

        // Reemplazar verdes por rojo
        bgr.setTo(Scalar(0, 0, 255), greenMask);

        // BGR → RGBA
        cvtColor(bgr, img, COLOR_BGR2RGBA);

        // Liberar memoria explícitamente
        bgr.release();
        hsv.release();
        greenMask.release();

        LOGD("Filtro Verde a Rojo aplicado");
    }
}
// ============================================================
// AGREGA ESTO AL FINAL DE TU native-lib.cpp EXISTENTE
// (después del último bloque de la función procesarFiltroC)
// ============================================================

// -------------------------------------------------------
// PUNTO 1: Detección de Rostros (Haar Cascade / Viola-Jones)
// -------------------------------------------------------
// IMPORTANTE: Antes de compilar debes copiar el archivo
// haarcascade_frontalface_default.xml a:
//   app/src/main/assets/haarcascade_frontalface_default.xml
// Descárgalo de:
//   https://github.com/opencv/opencv/raw/master/data/haarcascades/haarcascade_frontalface_default.xml
// -------------------------------------------------------
extern "C"
JNIEXPORT void JNICALL
Java_com_example_parcial_MainActivity_detectarRostrosC(JNIEnv *env, jobject thiz,
                                                       jlong addrInput,
                                                       jstring rutaCascade) {
    Mat &img = *(Mat *) addrInput;

    if (img.empty()) {
        LOGD("detectarRostrosC: imagen vacía");
        return;
    }

    // Convertir la ruta del cascade a string C++
    const char *ruta = env->GetStringUTFChars(rutaCascade, nullptr);

    // Cargar el clasificador Haar
    CascadeClassifier detector;
    if (!detector.load(ruta)) {
        LOGD("detectarRostrosC: No se pudo cargar el cascade: %s", ruta);
        env->ReleaseStringUTFChars(rutaCascade, ruta);
        return;
    }
    env->ReleaseStringUTFChars(rutaCascade, ruta);

    // Convertir RGBA → Gris para la detección
    Mat gris;
    cvtColor(img, gris, COLOR_RGBA2GRAY);
    equalizeHist(gris, gris); // Mejora contraste para mejor detección

    // Detectar rostros
    std::vector<Rect> rostros;
    detector.detectMultiScale(
            gris,
            rostros,
            1.2,             // scaleFactor: búsqueda más selectiva
            6,               // minNeighbors: mucho más estricto para evitar falsos
            0,               // flags
            Size(100, 100),  // tamaño mínimo mayor
            Size()           // tamaño máximo (sin límite)
    );

    LOGD("detectarRostrosC: %zu rostro(s) detectado(s)", rostros.size());

    // Dibujar un rectángulo verde sobre cada rostro detectado
    for (const Rect &r : rostros) {
        // Rectángulo verde (RGBA: 0, 255, 0, 255)
        rectangle(img, r, Scalar(0, 255, 0, 255), 3);

        // Texto con número de rostro sobre el rectángulo
        std::string texto = "Rostro";
        putText(img, texto,
                Point(r.x, r.y - 10),
                FONT_HERSHEY_SIMPLEX,
                0.8,
                Scalar(0, 255, 0, 255),
                2);
    }

    // Si no detectó nada, mostrar aviso en pantalla
    if (rostros.empty()) {
        putText(img, "Sin rostros detectados",
                Point(20, 50),
                FONT_HERSHEY_SIMPLEX,
                1.0,
                Scalar(0, 0, 255, 255),
                2);
    }

    gris.release();
}


// -------------------------------------------------------
// PUNTO 2: Detección de Monedas Colombianas (Lógica de SebastianUrrego/AndroidOPVS)
// -------------------------------------------------------

struct HSVStats {
    double H, S, V;
};

static HSVStats getRingHSV(const Mat &rgba, int cx, int cy, int r, float minPct, float maxPct) {
    Mat bgr, hsv;
    cvtColor(rgba, bgr, COLOR_RGBA2BGR);
    cvtColor(bgr, hsv, COLOR_BGR2HSV);

    int rMin = (int)(r * minPct);
    int rMax = (int)(r * maxPct);

    vector<uchar> vH, vS, vV;
    vH.reserve(400); vS.reserve(400); vV.reserve(400);

    for (int dy = -rMax; dy <= rMax; dy += 2) {
        for (int dx = -rMax; dx <= rMax; dx += 2) {
            int distSq = dx * dx + dy * dy;
            if (distSq < rMin * rMin || distSq > rMax * rMax) continue;

            int px = cx + dx, py = cy + dy;
            if (px < 0 || py < 0 || px >= rgba.cols || py >= rgba.rows) continue;

            Vec3b p = hsv.at<Vec3b>(py, px);
            vH.push_back(p[0]);
            vS.push_back(p[1]);
            vV.push_back(p[2]);
        }
    }

    if (vH.empty()) return {0, 0, 0};

    size_t mid = vH.size() / 2;
    nth_element(vH.begin(), vH.begin() + mid, vH.end());
    nth_element(vS.begin(), vS.begin() + mid, vS.end());
    nth_element(vV.begin(), vV.begin() + mid, vV.end());

    return {(double)vH[mid], (double)vS[mid], (double)vV[mid]};
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_parcial_MainActivity_detectarMonedasC(JNIEnv *env, jobject thiz,
                                                       jlong addrInput) {
    Mat &rgba = *(Mat *) addrInput;

    if (rgba.empty()) {
        LOGD("detectarMonedasC: imagen vacía");
        return 0;
    }

    // PASO 1: Preprocesar
    Mat gray, blurred;
    cvtColor(rgba, gray, COLOR_RGBA2GRAY);
    GaussianBlur(gray, blurred, Size(9, 9), 2.0);
    medianBlur(blurred, blurred, 5);

    // PASO 2: HoughCircles
    int minR = (int)(rgba.cols * 0.04f);
    int maxR = (int)(rgba.cols * 0.30f);
    vector<Vec3f> rawCircles;
    HoughCircles(blurred, rawCircles, HOUGH_GRADIENT, 1.5, rgba.cols * 0.10f, 100, 115, minR, maxR);

    // PASO 3: NMS (Non-Maximum Suppression)
    vector<Vec3f> coins;
    for (const auto &c : rawCircles) {
        bool keep = true;
        for (const auto &a : coins) {
            float d = hypot(c[0] - a[0], c[1] - a[1]);
            if (d < (c[2] + a[2]) * 0.40f) {
                keep = false;
                break;
            }
        }
        if (keep) coins.push_back(c);
    }

    if (coins.empty()) return 0;

    // PASO 4: Analizar color HSV
    struct Coin {
        int cx, cy, r;
        double cS, eS, cH, eH;
        bool is500, is1000, isGold;
    };
    vector<Coin> detected;

    for (const auto &c : coins) {
        int cx = cvRound(c[0]), cy = cvRound(c[1]), r = cvRound(c[2]);
        if (cx - r < 0 || cy - r < 0 || cx + r >= rgba.cols || cy + r >= rgba.rows) continue;

        HSVStats cen = getRingHSV(rgba, cx, cy, r, 0.00f, 0.40f);
        HSVStats edg = getRingHSV(rgba, cx, cy, r, 0.68f, 0.90f);

        double cS = cen.S, eS = edg.S;
        double cH = cen.H, eH = edg.H;

        bool is1000 = (eS > cS + 10.0 && eS > 20.0);
        bool is500 = (cS > eS + 10.0 && cS > 20.0);

        double avgS = (cS + eS) / 2.0;
        double avgH = (cH + eH) / 2.0;
        bool isGold = (!is500 && !is1000 && avgS > 50.0 && avgH > 10.0 && avgH < 45.0);

        detected.push_back({cx, cy, r, cS, eS, cH, eH, is500, is1000, isGold});
    }

    // PASO 5: Estimación de escala relativa (px_to_mm)
    float px_to_mm = -1.0f;
    {
        float sum = 0.0f; int cnt = 0;
        for (const auto &coin : detected) if (coin.is500) { sum += (float)coin.r / 11.85f; cnt++; }
        if (cnt > 0) px_to_mm = sum / cnt;
    }
    if (px_to_mm < 0) {
        float sum = 0.0f; int cnt = 0;
        for (const auto &coin : detected) if (coin.is1000) { sum += (float)coin.r / 13.35f; cnt++; }
        if (cnt > 0) px_to_mm = sum / cnt;
    }
    if (px_to_mm < 0) {
        px_to_mm = rgba.cols / 140.0f;
    }

    // PASO 6: Clasificar y Dibujar
    long total = 0;
    int coinCount = 0;

    for (const auto &coin : detected) {
        long val = 0;
        string lbl = "";

        if (coin.is1000) {
            val = 1000;
            lbl = "$1.000";
        } else if (coin.is500) {
            val = 500;
            lbl = "$500";
        } else {
            float dia = (coin.r * 2.0f) / px_to_mm;
            if (coin.isGold) {
                if (dia > 21.65f) val = 100;
                else              val = 100;
            } else {
                if (dia > 21.0f) val = 200;
                else             val = 50;
            }
            lbl = "$" + to_string(val);
        }

        Scalar color = (val == 1000) ? Scalar(255, 180, 50, 255) :
                       (val == 500)  ? Scalar(0, 200, 255, 255) :
                       (val == 200)  ? Scalar(220, 220, 220, 255) :
                       (val == 100)  ? Scalar(50, 160, 255, 255) :
                                       Scalar(30, 100, 200, 255);

        circle(rgba, Point(coin.cx, coin.cy), coin.r, Scalar(255, 255, 0, 255), 3);
        circle(rgba, Point(coin.cx, coin.cy), 5, color, FILLED);
        putText(rgba, lbl, Point(coin.cx - coin.r/2, coin.cy - 10), FONT_HERSHEY_SIMPLEX, 0.8, color, 2);

        if (val > 0) { total += val; coinCount++; }
    }

    string totalStr = "Total: $" + to_string(total) + " COP (" + to_string(coinCount) + ")";
    putText(rgba, totalStr, Point(10, 45), FONT_HERSHEY_SIMPLEX, 1.2, Scalar(0, 255, 0, 255), 3);

    return (jint)total;
}