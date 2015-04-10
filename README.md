tenhou-log-collector
====================

A tool for collecting log links from Tenhou's Mahjong clients.

Both Tenhou's Windows client and flash clients remove old game links. This tool lets you save those links in a separate file so that they never get deleted. It will look for logs in Flash's local storage as well as Windows client's directory. Collected game links are stored in the file logs.csv, in the same directory as the tool.

```
Usage: tenhoulogcollector [/nowait]
        /nowait        Do not require the user to press a key at the end of the program
```

Operating systems supported: Windows XP or higher, Mac OS 9 or higher, and most variants of Unix/Linux.

For Unix/Linux, compile the program by typing 'make'. If there are errors then your operating system is not supported.

Contact: hideki.nakeda@gmail.com