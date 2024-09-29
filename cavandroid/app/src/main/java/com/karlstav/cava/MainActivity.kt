package com.karlstav.cava

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.display.DisplayManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.opengl.GLES30
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Display
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuItem
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.WindowInsetsController
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceManager
import androidx.preference.SeekBarPreference
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import java.nio.IntBuffer
import java.nio.charset.StandardCharsets
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import java.util.concurrent.locks.ReentrantLock


private const val FLOAT_SIZE_BYTES = 4 // size of Float
private const val VERTICES_DATA_STRIDE_BYTES = 5 * FLOAT_SIZE_BYTES


private const val UNKNOWN_PROGRAM = -1
private const val UNKNOWN_ATTRIBUTE = -1

private val TAG = "cava_log"


private var numberOfBarsSet = 40
private var lowCutoffSet = 50
private var highCutoffSet = 1000
private var showFrequency = false
private var fgColor1RedSet = 0
private var fgColor1GreenSet = 0
private var fgColor1BlueSet = 0
private var fgColor2RedSet = 0
private var fgColor2GreenSet = 0
private var fgColor2BlueSet = 0
private var bgColorRedSet = 0
private var bgColorGreenSet = 0
private var bgColorBlueSet = 0
private var colorPreset = "1"
private var changeColor = true
private var configReload = false

//Define AudioRecord Object and other parameters
private const val RECORDER_SAMPLE_RATE = 44100
private const val  AUDIO_SOURCE = MediaRecorder.AudioSource.MIC
//for raw audio can use
private const val  RAW_AUDIO_SOURCE = MediaRecorder.AudioSource.UNPROCESSED
private const val  CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_MONO
private const val  AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
private var  BUFFER_SIZE_RECORDING = AudioRecord.getMinBufferSize(RECORDER_SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
private const val  MAX_BARS = 256
private var cutOffFrequencies = FloatArray(MAX_BARS + 1);


private val audioData = ByteArray(BUFFER_SIZE_RECORDING / 2)
private val barsData = FloatArray(MAX_BARS)
private val cavaData = DoubleArray(BUFFER_SIZE_RECORDING * 2)
private var newSamples = 0
private var refreshRate = 0
val lock = ReentrantLock()
class MainActivity : AppCompatActivity() {
    private lateinit var audioRecord:AudioRecord
    private lateinit var gLView: GLSurfaceView
    private val RECORD_AUDIO_PERMISSION_REQUEST_CODE = 101
    private var RECORD_AUDIO_PERMISSION_DENIED = false
    private var RECORDING = false
    private var settingsMenuIsOpen = false


    public override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        numberOfBarsSet = preferences.getInt("number_of_bars_preference_key", 40)
        showFrequency = preferences.getBoolean("show_frequency", false)
        lowCutoffSet = preferences.getInt("low_cut_off_frequency", 50)
        highCutoffSet = preferences.getInt("high_cut_off_frequency", 10000)
        fgColor1RedSet = preferences.getInt("fg_col_1_r_preference_key", 0)
        fgColor1GreenSet = preferences.getInt("fg_col_1_g_preference_key", 255)
        fgColor1BlueSet = preferences.getInt("fg_col_1_b_preference_key", 255)
        fgColor2RedSet = preferences.getInt("fg_col_2_r_preference_key", 255)
        fgColor2GreenSet = preferences.getInt("fg_col_2_g_preference_key", 0)
        fgColor2BlueSet = preferences.getInt("fg_col_2_b_preference_key", 0)
        bgColorRedSet = preferences.getInt("bg_col_r_preference_key", 0)
        bgColorGreenSet = preferences.getInt("bg_col_g_preference_key", 0)
        bgColorBlueSet = preferences.getInt("bg_col_b_preference_key", 0)
        colorPreset = preferences.getString("color_presets", "1").toString()

        if (highCutoffSet - lowCutoffSet < lowCutoffSet) {
            highCutoffSet = 10000
            lowCutoffSet = 50
        }

        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM);

        val display = getSystemService<DisplayManager>()?.getDisplay(Display.DEFAULT_DISPLAY)

        if (display != null) {
            refreshRate = display.refreshRate.toInt()
        }

        val windowInsetsController = window.decorView.windowInsetsController
        if (windowInsetsController != null) {
            // Hide the status bar and navigation bar
            windowInsetsController.hide(WindowInsets.Type.systemBars())
            windowInsetsController.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        Handler(Looper.getMainLooper()).postDelayed({
            if (!settingsMenuIsOpen)
                supportActionBar?.hide()
        }, 3000)

        gLView = MyGLSurfaceView(this)
        setContentView(gLView)
    }

    override fun onResume() {
        super.onResume()

        if (!RECORD_AUDIO_PERMISSION_DENIED) {
            if (checkPermission(Manifest.permission.RECORD_AUDIO)) {
                if (!RECORDING)
                    startRecording()
            } else {
                requestPermission(
                    Manifest.permission.RECORD_AUDIO,
                    RECORD_AUDIO_PERMISSION_REQUEST_CODE
                )
            }
        }
    }

    private fun checkPermission(permission: String): Boolean {
        return ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
    }

    private fun requestPermission(permission: String, requestCode: Int) {
        ActivityCompat.requestPermissions(this, arrayOf(permission), requestCode)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            RECORD_AUDIO_PERMISSION_REQUEST_CODE -> {
                RECORD_AUDIO_PERMISSION_DENIED =
                    !(grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun startRecording() {
        audioRecord = AudioRecord(AUDIO_SOURCE, RECORDER_SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT, BUFFER_SIZE_RECORDING)
        if (audioRecord!!.state != AudioRecord.STATE_INITIALIZED) {
            Log.e(TAG, "error initializing AudioRecord");
            return
        }
        audioRecord.startRecording()
        RECORDING = true
        Thread {
            writeAudioDataToBuffer()
        }.start()

    }

    private fun writeAudioDataToBuffer() {
        while (true) {
            val read = audioRecord!!.read(audioData, 0, audioData.size)
            //newSamples=0
            lock.lock()
            for (i in 0 until read step 2) {
                if (i > audioData.size)
                    break
                val lowByte = audioData[i].toInt()
                val highByte = audioData[i + 1].toInt()
                val pcmValue = (highByte shl 8) or (lowByte and 0xFF)
                cavaData[newSamples] = pcmValue.toDouble() / 32767.0
                newSamples++
                if (newSamples > cavaData.size - 1)
                    newSamples = 0
            }
            lock.unlock()
            if (!RECORDING)
                break
        }
        audioRecord.stop()
    }

    override fun onPause() {
        super.onPause()
        RECORDING = false
    }

    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.menu,menu)
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId){
            R.id.settings -> {
                if (!settingsMenuIsOpen) {
                    supportFragmentManager
                        .beginTransaction()
                        .replace(android.R.id.content, MySettingsFragment())
                        .commit()
                    settingsMenuIsOpen = true
                } else {
                    val fragment = supportFragmentManager.findFragmentById(android.R.id.content)
                    if (fragment != null) {
                        supportFragmentManager
                            .beginTransaction()
                            .remove(fragment)
                            .commit()
                        supportActionBar?.hide()
                        settingsMenuIsOpen = false
                    }
                }
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }
    override fun onTouchEvent(e: MotionEvent): Boolean {
        supportActionBar?.show()
        Handler(Looper.getMainLooper()).postDelayed({
            if (!settingsMenuIsOpen)
                supportActionBar?.hide()
        }, 3000)
        return true
    }

}
class MyGLSurfaceView(context: Context) : GLSurfaceView(context) {
    private val renderer: MyGLRenderer
    init {
        setEGLContextClientVersion(3)
        renderer = MyGLRenderer(context)
        setRenderer(renderer)
    }
}

class MyGLRenderer(private val context: Context) : GLSurfaceView.Renderer {

    private val mVertices: FloatBuffer
    private val bars: FloatBuffer
    private var numberOfBars = 40
    private var screenWidth: Int = 0
    private var screenHeight: Int = 0
    external fun InitCava(bars: Int, sampleRate: Int, lowerCutoff: Int, higherCutoff: Int): FloatArray
    external fun ExecCava(audioIn: DoubleArray, numSamples: Int): DoubleArray
    external fun DestroyCava()

    private var uniformBars = UNKNOWN_ATTRIBUTE
    private var uniformBarsCount = UNKNOWN_ATTRIBUTE
    private var uniformResolution = UNKNOWN_ATTRIBUTE
    private var uniformGradientCount = UNKNOWN_ATTRIBUTE
    private var uniformColors = UNKNOWN_ATTRIBUTE
    private var inPositionHandle = UNKNOWN_ATTRIBUTE
    private var uniformxCount = UNKNOWN_ATTRIBUTE
    private var uniformxLabels = UNKNOWN_ATTRIBUTE
    private var uniformxOn = UNKNOWN_ATTRIBUTE


init {
    System.loadLibrary("cavacore")
    program = UNKNOWN_PROGRAM

    val mVerticesData = floatArrayOf(
        // X, Y, Z, U, V
        -1.0f, -1.0f, 0f, 0f, 0f,
        1.0f, -1.0f, 0f, 1f, 0f,
        -1.0f,  1.0f, 0f, 0f, 1f,
        1.0f,  1.0f, 0f, 1f, 1f )

    mVertices = ByteBuffer.allocateDirect(mVerticesData.count() * FLOAT_SIZE_BYTES).order(ByteOrder.nativeOrder()).asFloatBuffer()
    mVertices.put(mVerticesData).position(0)

    bars =
        ByteBuffer.allocateDirect(MAX_BARS * FLOAT_SIZE_BYTES).order(ByteOrder.nativeOrder()).asFloatBuffer()

    numberOfBars = numberOfBarsSet
}

    override fun onSurfaceCreated(unused: GL10, config: EGLConfig) {
        // Set the background frame color
        val resource = context.resources

        val vertexShaderSource = getFileText(resource.openRawResource(R.raw.pass_through))
        val fragmentShaderSource = getFileText(resource.openRawResource(R.raw.bar_spectrum))

        createProgram(vertexShaderSource, fragmentShaderSource)
        checkGlError("createProgram ")
        uniformBars = GLES30.glGetUniformLocation(program, "bars")
        uniformBarsCount = GLES30.glGetUniformLocation(program, "bars_count")
        uniformResolution = GLES30.glGetUniformLocation(program, "u_resolution")
        uniformGradientCount = GLES30.glGetUniformLocation(program, "gradient_count")
        uniformColors = GLES30.glGetUniformLocation(program, "colors")
        uniformxCount = GLES30.glGetUniformLocation(program, "x_count")
        uniformxLabels = GLES30.glGetUniformLocation(program, "x_labels")
        uniformxOn = GLES30.glGetUniformLocation(program, "x_on")
        checkGlError("glGetUniformLocation ")

        GLES30.glUseProgram(program)
        checkGlError("glUseProgram ")

        inPositionHandle = GLES30.glGetAttribLocation(program, "vertexPosition_modelspace")
        GLES30.glVertexAttribPointer(inPositionHandle, 3, GLES30.GL_FLOAT, false, VERTICES_DATA_STRIDE_BYTES, mVertices)
        checkGlError("glVertexAttribPointer maPosition")
        GLES30.glEnableVertexAttribArray(inPositionHandle)
        checkGlError("glVertexAttribEnable maPosition")

        GLES30.glUniform1i(uniformBarsCount, numberOfBars)
        if (showFrequency)
            GLES30.glUniform1i(uniformxOn, 1)
        else
            GLES30.glUniform1i(uniformxOn, 0)

        GLES30.glUniform1i(uniformGradientCount, 2)
        cutOffFrequencies = InitCava(numberOfBars, refreshRate, lowCutoffSet, highCutoffSet)

    }

    override fun onDrawFrame(unused: GL10) {
        if(changeColor) {
            var gradientColorData = FloatArray(9)
            if (colorPreset != "1") {
                val backColor = colorPreset.substring(0, 7)
                val col1 = colorPreset.substring(7,14)
                val col2 = colorPreset.substring(14,21)
                gradientColorData = floatArrayOf(
                    floatColor(backColor.substring(1, 3).toInt(16)),
                    floatColor(backColor.substring(3, 5).toInt(16)),
                    floatColor(backColor.substring(5, 7).toInt(16)),
                    floatColor(col1.substring(1, 3).toInt(16)),
                    floatColor(col1.substring(3, 5).toInt(16)),
                    floatColor(col1.substring(5, 7).toInt(16)),
                    floatColor(col2.substring(1, 3).toInt(16)),
                    floatColor(col2.substring(3, 5).toInt(16)),
                    floatColor(col2.substring(5, 7).toInt(16))
                )
            } else {
                gradientColorData = floatArrayOf(
                    floatColor(bgColorRedSet),
                    floatColor(bgColorGreenSet),
                    floatColor(bgColorBlueSet),
                    floatColor(fgColor1RedSet),
                    floatColor(fgColor1GreenSet),
                    floatColor(fgColor1BlueSet),
                    floatColor(fgColor2RedSet),
                    floatColor(fgColor2GreenSet),
                    floatColor(fgColor2BlueSet)
                )
            }
                val gradientColors: FloatBuffer =
                    ByteBuffer.allocateDirect(9 * FLOAT_SIZE_BYTES).order(ByteOrder.nativeOrder())
                        .asFloatBuffer()
                gradientColors.put(gradientColorData).position(0)
                GLES30.glUniform3fv(uniformColors, 9, gradientColors)

            changeColor = false
        }
        if (showFrequency)
            GLES30.glUniform1i(uniformxOn, 1)
        else
            GLES30.glUniform1i(uniformxOn, 0)
        // Redraw background color
        GLES30.glClearColor(0.0f, 0.0f, 0.0f, 1.0f)
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT)

       //if (newSamples > 0) { //only draw when new data. maybe?
            lock.lock()
            var cavaOut = ExecCava(cavaData, newSamples);
            lock.unlock()
            for (i in 0 until numberOfBars) {
                barsData[i] = cavaOut[i].toFloat()
            }
            bars.put(barsData).position(0)
            newSamples = 0
       // }
        GLES30.glUniform4fv(uniformBars, numberOfBars, bars)
        checkGlError("glGetUniformfv uniform_bars")

        GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0, 4)
        checkGlError("glDrawArrays")

        if (configReload) {
            numberOfBars = numberOfBarsSet

            DestroyCava()
            cutOffFrequencies = InitCava(numberOfBars, refreshRate, lowCutoffSet, highCutoffSet)

            GLES30.glUniform1i(uniformBarsCount, numberOfBars)
            for (i in 0 until numberOfBars ) {
                barsData[i] = 0.0.toFloat()
            }

            makeLabels(screenWidth, screenHeight)

            configReload = false
        }

    }

    override fun onSurfaceChanged(unused: GL10, width: Int, height: Int) {
        GLES30.glViewport(0, 0, width, height)
        GLES30.glUniform3f(uniformResolution, width.toFloat(), height.toFloat(), 0.0f)
        checkGlError("glUniform3f uniform_u_resolution")
        screenWidth = width
        screenHeight = height
        makeLabels(width,height)
    }

    private fun makeLabels(width: Int, height: Int){

        var xCount = width / 200;

        if (xCount > 16)
            xCount = 16

        var xLabels = IntArray(xCount)

        for (i in 0 until xCount) {
            var labelIdx = ((i.toFloat() / (xCount -1).toFloat()) * numberOfBars).toInt()
            if (labelIdx > MAX_BARS + 1)
                labelIdx = MAX_BARS + 1

            xLabels[i] = cutOffFrequencies[labelIdx].toInt()
            if ( xLabels[i] > 100 && xLabels[i] < 1000)
                xLabels[i] = xLabels[i] / 10 * 10
            else if ( xLabels[i] > 1000)
                xLabels[i] = xLabels[i] / 100 * 100

        }

        xLabels[0] = cutOffFrequencies[0].toInt()

        var xLabelLength = xCount * 5 + xCount * 2
        var xLabelsString = IntArray(xLabelLength)

        //to avoid some fancy text in shader lib i implemented a 7-segment display in the shader
        //this converts the x axis labels to an int array of single digit numbers with -1 as padding and leading zeros

        for (i in 0 until xCount) {
            var label = xLabels[i]
            if (label >= 10000)
                xLabelsString[i * 7] = label / 10000
            else
                xLabelsString[i * 7] = -1

            var labelValue = (label / 10000) * 10000

            if (label >= 1000)
                xLabelsString[i * 7 + 1] = (label - labelValue) / 1000
            else
                xLabelsString[i * 7 + 1] = -1

            labelValue =+ (label / 1000) * 1000

            if (label >= 100)
                xLabelsString[i * 7 + 2] = (label - labelValue) / 100
            else
                xLabelsString[i * 7 + 2] = -1

            labelValue =+ (label / 100) * 100

            xLabelsString[i * 7 + 3] = (label - labelValue) / 10
            labelValue =+ (label / 10) * 10
            xLabelsString[i * 7 + 4] = (label - labelValue) / 1
            xLabelsString[i * 7 + 5] = -1
            xLabelsString[i * 7 + 6] = -1
        }
        GLES30.glUniform1i(uniformxCount, xLabelLength)
        val xLabelsBuffer: IntBuffer =
            ByteBuffer.allocateDirect(xLabelLength * 4).order(ByteOrder.nativeOrder())
                .asIntBuffer()
        xLabelsBuffer.put(xLabelsString).position(0)
        GLES30.glUniform1iv(uniformxLabels, xLabelLength, xLabelsBuffer)

    }

    private fun floatColor(color: Int): Float {
        return color.toFloat() / 255.toFloat()
    }

}
private var program = UNKNOWN_PROGRAM
private fun getFileText(inputStream: InputStream):String {
    val bytes = inputStream.readBytes()        //see below
    val text = String(bytes, StandardCharsets.UTF_8)    //specify charset
    inputStream.close()
    return text
}
private fun createProgram(vertexSource: String, fragmentSource: String): Boolean {
    if (program != UNKNOWN_PROGRAM) {
        // delete program
        GLES30.glDeleteProgram(program)
        checkGlError("glAttachShader: delete program")
        program = UNKNOWN_PROGRAM
    }
    // load vertex shader
    val vertexShader = loadShader(GLES30.GL_VERTEX_SHADER, vertexSource)
    if (vertexShader == UNKNOWN_PROGRAM) {
        return false
    }
    // load pixel shader
    val pixelShader = loadShader(GLES30.GL_FRAGMENT_SHADER, fragmentSource)
    if (pixelShader == UNKNOWN_PROGRAM) {
        return false
    }
    program = GLES30.glCreateProgram()
    if (program != UNKNOWN_PROGRAM) {
        GLES30.glAttachShader(program, vertexShader)
        checkGlError("glAttachShader: vertex")
        GLES30.glAttachShader(program, pixelShader)
        checkGlError("glAttachShader: pixel")
        return linkProgram()
    }
    return true
}

private fun linkProgram(): Boolean {
    if (program == UNKNOWN_PROGRAM) {
        return false
    }
    GLES30.glLinkProgram(program)
    val linkStatus = IntArray(1)
    GLES30.glGetProgramiv(program, GLES30.GL_LINK_STATUS, linkStatus, 0)
    if (linkStatus[0] != GLES30.GL_TRUE) {
        Log.e(TAG, "Could not link program: ")
        Log.e(TAG, GLES30.glGetProgramInfoLog(program))
        GLES30.glDeleteProgram(program)
        program = UNKNOWN_PROGRAM
        return false
    }
    return true
}

private fun loadShader(shaderType: Int, source: String): Int {
    var shader = GLES30.glCreateShader(shaderType)
    if (shader != UNKNOWN_PROGRAM) {
        GLES30.glShaderSource(shader, source)
        GLES30.glCompileShader(shader)
        val compiled = IntArray(1)
        GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, compiled, 0)
        if (compiled[0] == UNKNOWN_PROGRAM) {
            Log.e(TAG, "Could not compile shader $shaderType:")
            Log.e(TAG, GLES30.glGetShaderInfoLog(shader))
            GLES30.glDeleteShader(shader)
            shader = UNKNOWN_PROGRAM
        }
    }
    return shader
}

private fun checkGlError(op: String) {
    var error: Int
    while (GLES30.glGetError().also { error = it } != GLES30.GL_NO_ERROR) {
        Log.e(TAG, "$op: glError $error")
        throw RuntimeException("$op: glError $error")
    }
}

class MySettingsFragment : PreferenceFragmentCompat() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val view = super.onCreateView(inflater, container, savedInstanceState)
        view?.setBackgroundColor(0x77000000.toInt()) // make background slightly opaque
        return view
    }
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
        val numberOfBarsPref = findPreference<Preference>("number_of_bars_preference_key")
        numberOfBarsPref?.setOnPreferenceChangeListener { reference, newValue ->
            numberOfBarsSet = newValue as Int
            configReload = true
            true
        }
        val lowCuttoffpref = findPreference<Preference>("low_cut_off_frequency")
        val highCuttoffpref = findPreference<Preference>("high_cut_off_frequency")

        lowCuttoffpref?.setOnPreferenceChangeListener { reference, newValue ->
            lowCutoffSet = newValue as Int
            if (highCuttoffpref is SeekBarPreference) {
                highCuttoffpref.min = lowCutoffSet * 2
                if (highCutoffSet < lowCutoffSet * 2)
                    highCuttoffpref.value = lowCutoffSet * 2
            }
            configReload = true
            true
        }
        highCuttoffpref?.setOnPreferenceChangeListener { reference, newValue ->
            highCutoffSet = newValue as Int
            if (lowCuttoffpref is SeekBarPreference) {
                lowCuttoffpref.max = highCutoffSet / 2
                if (lowCutoffSet > highCutoffSet / 2)
                    lowCuttoffpref.value = highCutoffSet / 2
            }
            configReload = true
            true
        }
        if (lowCuttoffpref is SeekBarPreference) {
            lowCuttoffpref.max = highCutoffSet / 2
        }
        if (highCuttoffpref is SeekBarPreference) {
            highCuttoffpref.min = lowCutoffSet * 2
        }
        val bandWidthDefault = findPreference<Preference>("bandwidth_default")
        bandWidthDefault?.setOnPreferenceClickListener {
            if (lowCuttoffpref is SeekBarPreference) {
                lowCutoffSet = 50
                lowCuttoffpref.max = 10000
                lowCuttoffpref.value = lowCutoffSet
            }
            if (highCuttoffpref is SeekBarPreference) {
                highCutoffSet = 10000
                highCuttoffpref.min = 100
                highCuttoffpref.value = highCutoffSet
            }
            configReload = true
            true
        }
        val showFreqPref = findPreference<Preference>("show_frequency")
        showFreqPref?.setOnPreferenceChangeListener { reference, newValue ->
            showFrequency = newValue as Boolean
            true
        }
        val fgColor1RedPref = findPreference<Preference>("fg_col_1_r_preference_key")
        fgColor1RedPref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor1RedSet = newValue as Int
            changeColor = true
            true
        }
        val fgColor1GreenPref = findPreference<Preference>("fg_col_1_g_preference_key")
        fgColor1GreenPref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor1GreenSet = newValue as Int
            changeColor = true
            true
        }
        val fgColor1BluePref = findPreference<Preference>("fg_col_1_b_preference_key")
        fgColor1BluePref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor1BlueSet = newValue as Int
            changeColor = true
            true
        }
        val fgColor2RedPref = findPreference<Preference>("fg_col_2_r_preference_key")
        fgColor2RedPref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor2RedSet = newValue as Int
            changeColor = true
            true
        }
        val fgColor2GreenPref = findPreference<Preference>("fg_col_2_g_preference_key")
        fgColor2GreenPref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor2GreenSet = newValue as Int
            changeColor = true
            true
        }
        val fgColor2BluePref = findPreference<Preference>("fg_col_2_b_preference_key")
        fgColor2BluePref?.setOnPreferenceChangeListener { reference, newValue ->
            fgColor2BlueSet = newValue as Int
            changeColor = true
            true
        }
        val bgColorRedPref = findPreference<Preference>("bg_col_r_preference_key")
        bgColorRedPref?.setOnPreferenceChangeListener { reference, newValue ->
            bgColorRedSet = newValue as Int
            changeColor = true
            true
        }
        val bgColorGreenPref = findPreference<Preference>("bg_col_g_preference_key")
        bgColorGreenPref?.setOnPreferenceChangeListener { reference, newValue ->
            bgColorGreenSet = newValue as Int
            changeColor = true
            true
        }
        val bgColorBluePref = findPreference<Preference>("bg_col_b_preference_key")
        bgColorBluePref?.setOnPreferenceChangeListener { reference, newValue ->
            bgColorBlueSet = newValue as Int
            changeColor = true
            true
        }
        val colorPresetPref = findPreference<Preference>("color_presets")
        colorPresetPref?.setOnPreferenceChangeListener { reference, newValue ->
            colorPreset = newValue as String
            changeColor = true
            true
        }

    }
}