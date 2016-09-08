# Mouse
A simple .mp4 video player written with SDL2 and FFMpeg

This program was written on Windows 7 x64 using the Microsoft MSVC compiler.
I cannot guarantee it will run on any other system.

This project was intended for me to learn a bit more about video playback, file formats, and codecs. I wanted a simple video player that could frame-by-frame seek through a .mp4 video _both forwards and backwards_. VLC cannot do this (it can do frame-forwards) and PotPlayer can be very buggy in this regard.

Although I have tested out on quite a few different formats, Mouse really works best on files that are AAC h264/x264 encoded with 3 ReFrames at a profile of high. Other formats may give an couple of errors to stderr with "bad src image pointers" even though the video will decode just fine.

This is not indended to be a full featured video player. It can play video and it can seek frame by frame. That's it. No audio, no frills.

Use at your own risk.
