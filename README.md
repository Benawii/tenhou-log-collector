tenhou-log-collector
====================

A tool for collecting log links from tenhou's Mahjong client.

It will look for logs in Flash's local storage as well as Windows client's directory. Collected game links are stored in logs.csv, in the same directory as the tool.

```
Usage: tenhoulogcollector [/nowait]
        /nowait        Do not require the user to press a key at the end of the program
```

Operating systems supported: Windows XP or higher, most variants of Unix/Linux.

For Unix/Linux, compile the program by typing 'make'. If there are errors then your operating system is not supported.
