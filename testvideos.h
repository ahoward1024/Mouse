#ifndef TESTVIDEOS_H
#define TESTVIDEOS_H

// TEST VIDEO: 1 frame of RED, 2 frames of BLUE, 2 frames of GREEN, 1 frame of YELLOW
// 6 frames total, at 60 fps
// NOTE: testavi is UNCOMPRESSED!! testmp4 is compressed with the x264 codec
global const char *testraw  = "../res/video/test/testraw.avi";
global const char *testmp4  = "../res/video/test/testx264.mp4";
global const char *testavi  = "../res/video/test/testx264.avi";

// Exactly 240 black frames at 60 fps
global const char *black240raw  = "../res/video/test/black240raw.avi";
global const char *black240avi  = "../res/video/test/black240x264.avi";
global const char *black240mp4  = "../res/video/test/black240x264.mp4";

// TEST LONG FILE (WARNING: ONLY ON QUASAR)
global const char *pm4 =  "H:\\Fraps\\Movies\\MISC\\pm4 - 2.avi";

// TEST VIDEO FILES
global const char *anime404       = "../res/video/Anime404.mp4";
global const char *a404test       = "../res/video/a404test.mp4";
global const char *dance          = "../res/video/dance.mp4";
global const char *nggyu          = "../res/video/nggyu.mp4";
global const char *vapor          = "../res/video/vapor.mp4";
global const char *watamote       = "../res/video/watamote.mp4";
// TEST AUDIO FILES
global const char *nevercomedown  = "../res/audio/nevercomedown.mp3";
global const char *ruskie         = "../res/audio/ruskie.webm";
global const char *someonenew     = "../res/audio/someonenew.mp3";
global const char *switchfriends  = "../res/audio/switchfriends.wav";

#endif