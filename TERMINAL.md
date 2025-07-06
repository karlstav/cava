Cava can now also run in an old [dumb terminal](https://en.wikipedia.org/wiki/Computer_terminal)!

As opposed to a terminal emulator or virtual consoles (tty1-7), this is the real deal.
A monitor and keyboard connected to a mainframe computer via serial cable.

# Step 1: Get a terminal
If you do not already own one, you can get lucky on various online marketplaces world wide.
I was able to get my hands on a [HP 700/96](https://terminals-wiki.org/wiki/index.php/HP_700/96).
Make sure it supports VT200 mode (or more recent maybe?) Sometimes called EM220 mode.
Must support at least 38400 baud, preferably more but I haven't been able to find one that supports it, would be interesting to test.

# Step 2: Connect
Terminals usually communicate with the computer over a RS232 serial interface. Modern motherboards still have these,
but often they must be mapped out via a 10 Pin Motherboard Header to DB9 connector bracket.
Alternatively you can get a USB to RS232 adapter, but my experience was better when connecting directly to the motherboard.
In my case both the computer and the terminal had Data Terminal Equipment (DTE) male connectors so i also needed a [null modem](https://en.wikipedia.org/wiki/Null_modem).

# Step 3: Login
Most of the documentation I could find was for some strange reason very much outdated.
But I was able to find some modern [documentaion](https://0pointer.de/blog/projects/serial-console.html).
When using a USB adapter I had to add `--autologin username` in order to log in, or it would stall forever on the password prompt.
It would actually also cause some strange behavior in my gui session that would not fix itself until i unplugged the USB adapter.
All this was no issue when connecting directly to the motherboard
The datacom settings (baud rate, parity, flow control) must match on the terminal and computer side.

# Step 4: Optimize
A terminal usually have 80x24 cells, so writing a character to each of them would mean 80 x 24 x 8 = 15360 bits.
With only 30 fps that is 460800 bit/s, a lot more than is supported over a RS232 connection (typically max 115200).
My HP 700/96 only supported 38400 bit/s, so at 30 fps that would limit me to only 80 chars per frame, or one of the 24 lines in the terminal.
Cava does do some heavy optimization like not rewriting a cell if it's the same as the last frame, but still the framerate would be very low without tweaking the settings.

I was able to make it look ok with these settings:

```
[general]
framerate = 45
max_height = 13
bar_width = 2
bar_spacing = 1
bars = 20
```

![terminal](https://github.com/karlstav/cava/blob/master/example_files/terminal.gif "terminal")

# Troubleshooting
## Font
If you are seing this:

![font_terminal_issue](https://github.com/karlstav/cava/blob/master/example_files/font_terminal_issue.jpg "font_terminal_issue")

It means the font did not load. Cava uses something called [Down-Line-Loadable Character Set](https://www.vt100.net/docs/vt220-rm/chapter4.html#S4.16)
to create its own font on the terminal at runtime.
Maybe your terminal does not support this feature?
## Bit errors
If you are seing this:

![bit_error_issue](https://github.com/karlstav/cava/blob/master/example_files/bit_error_issue.jpg "bit_error_issue")


You most likely have bit errors, check cabling or try reducing baud rate. When using a usb adapter i had reduce my bitrate to 19200 even though my terminal supported 38400.

## Latency
got latency? try optimizing more:

* disable everything color related, the terminal most likely does not support it anyway.
* lower framerate
* lower max_height
* fewer bars
* narrower bars
* less or no bar spacing

