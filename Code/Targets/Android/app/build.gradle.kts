plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.chillcore.engine"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "com.chillcore.engine"
        minSdk = 34
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        // v1 ships arm64-v8a only. Multi-ABI is a post-v1 concern.
        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                // Phase B/v1 ships GLES backend with DevUi disabled.
                arguments += listOf(
                    "-DCC_GFX_BACKEND_GLES=ON",
                    "-DCC_DISABLE_DEVUI=ON"
                )
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
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    sourceSets {
        getByName("main") {
            // Engine assets ship from the existing App/Data tree without
            // duplication. Path is relative to this build.gradle.kts —
            // Code/Targets/Android/app/ -> ../../../App/Data.
            assets.srcDirs("../../../App/Data")
        }
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.games.activity)
}
