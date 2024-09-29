import com.android.build.api.dsl.LintOptions

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    signingConfigs {
        getByName("debug") {
            storeFile = file("../keystore.jks")
            storePassword = "cava123"
            keyPassword = "cava123"
            keyAlias = "key0"
        }
        create("release") {
            storeFile = file("../keystore.jks")
            storePassword = "cava123"
            keyPassword = "cava123"
            keyAlias = "key0"
        }
    }
    namespace = "com.karlstav.cava"
    compileSdk = 33

    defaultConfig {
        applicationId = "com.karlstav.cava"
        minSdk = 30
        targetSdk = 33
        versionCode = 14
        versionName = "@string/app_ver"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }
        /*
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
        */
        externalNativeBuild {

            // For ndk-build, instead use the ndkBuild block.
            cmake {

                // Passes optional arguments to CMake.
                arguments("-DFFTW_DIR=${projectDir}/../../../fftw3-android")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            isDebuggable = false
            signingConfig = signingConfigs.getByName("release")
        }
    }
    // Encapsulates your external native build configurations.
    externalNativeBuild {

        // Encapsulates your CMake build configurations.
        cmake {
            // Provides a relative path to your CMake build script.
            path = file("../../CMakeLists.txt")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.4.3"
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }

}

dependencies {

    implementation("androidx.core:core-ktx:1.9.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.6.1")
    implementation("androidx.activity:activity-compose:1.7.0")
    implementation ("androidx.appcompat:appcompat:1.3.1")
    implementation("androidx.preference:preference-ktx:1.2.0")

    implementation(platform("androidx.compose:compose-bom:2023.03.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
    androidTestImplementation(platform("androidx.compose:compose-bom:2023.03.00"))
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-tooling")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
}