plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.parcial"
    compileSdk = 36 // Actualizado a 36 para cumplir con los requisitos de las librerías de AndroidX

    defaultConfig {
        applicationId = "com.example.parcial"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                // CORRECCIÓN: En .kts se usa .arguments.add() y .cppFlags()
                cppFlags("-std=c++17")

                // Si la línea de arriba te sigue dando error, prueba esta alternativa:
                // arguments("-DANDROID_CPP_FEATURES=rtti exceptions", "-std=c++17")

                arguments.add("-DOpenCV_DIR=${project.rootDir}/opencv-4.12.0-android-sdk/OpenCV-android-sdk/sdk/native/jni")

                // Importante: usar add() para cada arquitectura o addAll()
                abiFilters.addAll(listOf("armeabi-v7a", "arm64-v8a"))
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    // --- VINCULAR CON TU ARCHIVO CMAKELISTS ---
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.activity)
    implementation(libs.constraintlayout)

    // Si ya importaste OpenCV como modulo, esta linea esta bien:
    implementation(project(":openCV"))

    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)
    androidTestImplementation(libs.espresso.core)

    // CameraJhr y CameraX
    implementation("com.github.jose-jhr:Library-CameraX:1.0.8")
    implementation("androidx.camera:camera-view:1.3.0") // Versión más estable
    implementation("androidx.camera:camera-lifecycle:1.3.0")
}