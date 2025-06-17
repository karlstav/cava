Cava can now also run in an old [dumb terminal](https://en.wikipedia.org/wiki/Computer_terminal)!

As opposed to a terminal emulator or virtual consoles (tty1-7), this is the real deal.
A monitor and keyboard connected to a mainframe computer via serial cable.

# Step 1: Get a terminal
If you do not already own one, you can get lucky on various online marketplaces world wide.
I was able to get my hands on a [HP 700/96](https://terminals-wiki.org/wiki/index.php/HP_700/96).
Make sure it supports VT200 mode (or more recent maybe?) Sometimes called EM220 mode.
Must support at least 19200 baud, preferebly more but I haven't been able to find one, would be interseting to test.

# Step 2: Connect
Terminals usually communicate with the computer over a RS232 serial interface. Modern motherboards still have these,
but often they must be mapped out via extension boards. So alternativly you can do like me and get a USB to RS232 adapter.
In my case both the adapter and the terminal had Data Terminal Equipment (DTE) male connectors so i also needed a [null modem](https://en.wikipedia.org/wiki/Null_modem).

# Step 3: Login
Most of the documentation I could find was for some strange reason very much outdated. But I was able to find some modern [documentaion](https://0pointer.de/blog/projects/serial-console.html).
I had to add `--autologin username` in order to log in, or it would stall forever on the password prompt. 
It would actually also cause some strange behaviour in my gui session that would not fix itself until i unplugged the usb-rs232 adapter.
The datacom settings (baud rate, parity, flow control) must match on the terminal and computer side.

# Step 4: Optimize
A termimnal usually have 80x24 cells, so writing a charachter to each of them would mean 80 x 24 x 8 = 15360 bits. if you want 60 fps that is 921600 bit/s.
Typically a terminal would only have 9600 baud (bit/s), my HP should supported 38400 but only worked fine on 19200.
At 30 FPS that would limit me to only be able to modify at max 80 chars per frame, or one of the 24 lines in the terminal.
Cava does do some heavy optimisation like not rewriting a cell if it's the same as the last frame, but still the framerate would be verry poor without tweaking the settings.

Try optimizing these setting to decrease the data rate:
* disable everyting color releated, the terminal most likely does not support it anyway.
* lower framerate
* lower max_height
* fewer bars
* narrower bars
* less or no bar spacing

I was able to make it work at 3 lines (max_height = 15), 20 bars with 1 bar width and 1 bar spacing:

![terminal](https://github.com/karlstav/cava/blob/master/example_files/terminal.gif "terminal")

# Troubleshooting
## Font
If you are seing this:

![font_terminal_issue](https://github.com/karlstav/cava/blob/master/example_files/font_terminal_issue.jpg "font_terminal_issue")

It means the font did not load. Cava uses something called [Down-Line-Loadable Character Set](https://www.vt100.net/docs/vt220-rm/chapter4.html#S4.16)
to create its own font on the terminal at runtime.
Maybe your termminal does not support this feature?
## Bit errors
If you are seing this:

![bit_error_issue](https://github.com/karlstav/cava/blob/master/example_files/bit_error_issue.jpg "bit_error_issue")


You most likely have bit errors, check cabling or try reducing baud rate. I had reduce my bitrate to 19200 even though it suported 38400.

## Latency
got latency? try optimizing more.


