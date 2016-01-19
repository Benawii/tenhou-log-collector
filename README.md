tenhou-log-collector
====================

A tool for collecting log links from Tenhou's Mahjong clients.

Tenhou's Windows client and flash clients only store game links of 20-40 most recent games. This tool lets you save those links in a separate file so that they never get deleted. You can make it run on system startup (with /nowait flag) so that you will not have to worry about losing game links.

It will look for logs in Flash's local storage as well as Windows client's directory. Collected game links are stored in the file logs.csv, in the same directory as the tool. This file can be opened by Excel or any text editor such as Notepad.

```
Usage: tenhoulogcollector [/nowait]
        /nowait        Do not require the user to press a key at the end of the program
```

Operating systems supported: Windows XP or higher, Mac OS 9 or higher, and most variants of Unix/Linux.

For Windows, download tenhoulogcollector.exe and run it.


Contact: hideki.nakeda@gmail.com
