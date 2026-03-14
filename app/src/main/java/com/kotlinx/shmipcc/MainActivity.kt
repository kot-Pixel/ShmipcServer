package com.kotlinx.shmipcc

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.kotlinx.shmipcc.databinding.ActivityMainBinding
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        thread {
            stringFromJNI()
        }

    }

    /**
     * A native method that is implemented by the 'shmipcc' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'shmipcc' library on application startup.
        init {
            System.loadLibrary("shmipcc")
        }
    }
}