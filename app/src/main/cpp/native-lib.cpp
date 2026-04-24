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