# tenhou-log-collector

A tool for collecting log links and downloading actual log files from Tenhou's Mahjong clients.

Tenhou's flash and Windows clients only store game links of 40 most recent games. This tool will collect those links and save them in a separate file so you never lose them. It also downloads the actual log files to your computer. Set it to run automatically on system startup and you will never have to worry about losing game logs again. The Windows client can be configured to download the log files to your computer, so you would not need this tool if you use the Windows client.

The log collector will look for game logs in Flash's local storage as well as Windows client's directory. It does not work for the HTML client. Collected game links are stored in the comma-separated value file "logs.csv", in the same directory as the tool. This file can be opened by Excel or any text editor such as Notepad. Downloaded log files are stored in the directory "mjlog" by default.

The logs.csv file contains your rank and rating, among other things, so you can make graphs like this: 
![](https://github.com/Benawii/tenhou-log-collector/blob/master/sample/sample_rating_plot.png?raw=true)

```
Usage: tenhoulogcollector [--nowait] [-d directory] [--ascii]
	--nowait          Do not require the user to press a key at the end of the program
	-d, --directory   Specify directory to store the log files (default: "mjlog")
	--ascii           Use ASCII encoding for the logs.csv file instead of the default UTF-8
```

Operating systems supported: Windows XP and higher.

Simply download tenhoulogcollector.exe from the [Releases](https://github.com/Benawii/tenhou-log-collector/releases) page, move it to the desired directory, and run it.

Contact: hideki.nakeda@gmail.com
