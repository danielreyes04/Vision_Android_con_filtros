#include <jni.h>
#include <opencv2/opencv.hpp>
#include <android/log.h>

using namespace cv;

#define LOG_TAG "FiltrosC++"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

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
            1.1,     // scaleFactor
            3,       // minNeighbors
            0,       // flags
            Size(80, 80),   // tamaño mínimo de rostro
            Size()          // tamaño máximo (sin límite)
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
// PUNTO 2: Detección de Monedas Colombianas (HoughCircles)
// La clasificación es por radio en píxeles relativo al
// ancho de la imagen (así funciona con cualquier resolución)
// Monedas COP por diámetro real:
//   $50  → 17.0 mm
//   $100 → 20.3 mm
//   $200 → 22.4 mm
//   $500 → 23.7 mm
//   $1000→ 26.7 mm
// -------------------------------------------------------
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_parcial_MainActivity_detectarMonedasC(JNIEnv *env, jobject thiz,
                                                       jlong addrInput) {
    Mat &img = *(Mat *) addrInput;

    if (img.empty()) {
        LOGD("detectarMonedasC: imagen vacía");
        return 0;
    }

    // RGBA → Gris → Filtro para reducir ruido
    Mat gris, blur;
    cvtColor(img, gris, COLOR_RGBA2GRAY);

    // El desenfoque mediano es mejor para eliminar las líneas de la madera (ruido de sal y pimienta)
    medianBlur(gris, blur, 7);

    // Detectar círculos con HoughCircles
    std::vector<Vec3f> circulos;
    HoughCircles(
            blur,
            circulos,
            HOUGH_GRADIENT,
            1.2,                        // dp (resolución: 1.2 ayuda a ser más preciso)
            blur.rows / 10.0,           // minDist (separación entre monedas para evitar duplicados)
            140,                        // param1: umbral Canny (Solo bordes de metal muy fuertes)
            45,                         // param2: umbral acumulador (Equilibrado para la foto)
            blur.cols / 55,             // radio mínimo
            blur.cols / 8               // radio máximo (aumentado para permitir monedas grandes)
    );

    LOGD("detectarMonedasC: %zu círculo(s) detectado(s)", circulos.size());

    int totalPesos = 0;

    // Factor de escala: radio relativo al ancho de la imagen
    float anchoImg = (float) img.cols;

    for (const Vec3f &c : circulos) {
        Point centro(cvRound(c[0]), cvRound(c[1]));
        int radio = cvRound(c[2]);

        // Radio relativo (0.0 a 1.0) respecto al ancho
        float radioRel = (float) radio / anchoImg;

        // --- ANÁLISIS DE COLOR AVANZADO (CENTRO Y BORDE) ---
        // ROI 1: Centro (para bimetálicas y color base)
        Rect roiC(centro.x - 3, centro.y - 3, 7, 7);
        roiC &= Rect(0, 0, img.cols, img.rows);
        Scalar colorC = mean(img(roiC));

        // ROI 2: Borde (para detectar si es bimetálica)
        // Tomamos una muestra a la derecha del centro, casi al final del radio
        int offsetBorde = (int)(radio * 0.75);
        Rect roiB(centro.x + offsetBorde - 3, centro.y - 3, 7, 7);
        roiB &= Rect(0, 0, img.cols, img.rows);
        Scalar colorB = mean(img(roiB));

        // Analizamos saturación en HSV para ambos puntos
        Mat bgrC(1, 1, CV_8UC3, Scalar(colorC[2], colorC[1], colorC[0]));
        Mat bgrB(1, 1, CV_8UC3, Scalar(colorB[2], colorB[1], colorB[0]));
        Mat hsvC, hsvB;
        cvtColor(bgrC, hsvC, COLOR_BGR2HSV);
        cvtColor(bgrB, hsvB, COLOR_BGR2HSV);

        int satC = hsvC.at<Vec3b>(0,0)[1];
        int satB = hsvB.at<Vec3b>(0,0)[1];

        // Umbrales calibrados para luz de interiores
        bool centroDorado = (satC > 38);
        bool bordeDorado  = (satB > 38);

        // Clasificación por "Lógica de Material Estructural" basada en datos técnicos COP
        int valor = 0;
        if (satC > (satB + 10)) {
            // --- BIMETÁLICA: CENTRO ORO / ANILLO PLATA ($1000 o $500 Antigua) ---
            if (radioRel > 0.058f) valor = 1000;
            else                   valor = 500; // 500 Antigua
        }
        else if (satB > (satC + 10)) {
            // --- BIMETÁLICA: CENTRO PLATA / ANILLO ORO ($500 Nueva) ---
            valor = 500;
        }
        else if (centroDorado && bordeDorado) {
            // --- TODO DORADO ($100 Antigua o $100 Nueva) ---
            valor = 100;
        }
        else {
            // --- TODO PLATEADO ($50, $200 Nueva, $200 Antigua) ---
            if      (radioRel < 0.040f) valor = 50;
            else if (radioRel < 0.050f) valor = 100;
            else if (radioRel < 0.058f) valor = 200; // 200 nueva
            else                        valor = 200; // 200 antigua (más grande)
        }

        totalPesos += valor;

        // Dibujar círculo exterior (amarillo)
        circle(img, centro, radio, Scalar(255, 215, 0, 255), 3);

        // Dibujar punto central (rojo)
        circle(img, centro, 4, Scalar(255, 0, 0, 255), -1);

        // Mostrar el valor de la moneda encima del círculo
        std::string textoValor = "$" + std::to_string(valor);
        putText(img, textoValor,
                Point(centro.x - 25, centro.y - radio - 10),
                FONT_HERSHEY_SIMPLEX,
                0.8,
                Scalar(255, 215, 0, 255),
                2);
    }

    // Mostrar total en la esquina superior izquierda
    std::string textoTotal = "Total: $" + std::to_string(totalPesos) + " COP";
    putText(img, textoTotal,
            Point(10, 45),
            FONT_HERSHEY_SIMPLEX,
            1.2,
            Scalar(0, 255, 0, 255),
            3);

    // Si no detectó monedas
    if (circulos.empty()) {
        putText(img, "Sin monedas detectadas",
                Point(10, 50),
                FONT_HERSHEY_SIMPLEX,
                1.0,
                Scalar(0, 0, 255, 255),
                2);
    }

    gris.release();
    blur.release();

    return totalPesos;
}