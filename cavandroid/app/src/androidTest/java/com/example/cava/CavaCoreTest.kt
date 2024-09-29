package com.karlstav.cava

import androidx.test.ext.junit.runners.AndroidJUnit4

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class CavaCoreTest {
    external fun InitCava(pInt: Int): Int
    external fun ExecCava(pDoubleArray: DoubleArray, pInt: Int): DoubleArray

    @Test
    fun cavaExec_null_test() {
        System.loadLibrary("cavacore")

        val numberOfBars = 10
        val cavaData = DoubleArray(256)
        val barsData = FloatArray(10)


        for (i in 0 until 256) {
            cavaData[i] = 0.0
        }
        val rc = InitCava(numberOfBars)

        var cavaOut = ExecCava(cavaData, 256);


        for (i in 0 until numberOfBars) {
            assertEquals(0.0f, barsData[0])
        }
    }
}