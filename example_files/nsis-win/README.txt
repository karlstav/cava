Not boring and kinda important README file!
-------------------------------------------

Once CAVA-G is installed, you should configure the input device from which it will record audio (your output).
Here are instructions on how to do that:

0) Get a text editor that isn't notepad (must support LF and CRLF line endings), Notepad++ would do the job just fine

1) Open the following file %APPDATA%\cava\config

2) Navigate to the end of the file

3) The line at the end that says "; source = something", remove it's semicolon (DON'T CLOSE THE FILE YET, we still need it)

4) Open C:\Program Files (x86)\cava\devices.exe, it will list all of your available output/input devices

5) Find the one that says 'Stereo Mix' or something similar and memorise it's number.

   If you don't have it:

   5.1) Check if you haven't accidentally or by default disabled it
        
        Right click the speaker from the bottom right then go to 'Recording devices'
        Right click on the new window and check 'Show disabled devices'
        And if it's there, enable it!

    If it still isn't there:
    
    5.2) Get better drivers from the manufacurers website, by default Microsoft audio drivers are shit to be honest (just developer things)

    5.3) Get a better sound card

    5.4) Get something that is newer than Windows XP (like seriously)

6) Set the 'source' to that number (fxp. mine is #12, so it will be 'source = 12')

7) Change other stuff around if you want...

8) Save, exit and run CAVA. And you are done....


Hopefully this process isn't this tedious in the near future.
Anyway enjoy using my software (or don't) and thanks for using it btw.

For additional info on how to's and if you want to dig into the project, go here: https://github.com/nikp123/cava

Also this project runs on Linux and BSD (unofficially macOS, too), so if you are interested you can install on them too.

And the legal stuff is in 'LICENSE.txt' in the installation directory