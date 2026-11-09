This is strome. A new browser where each tab is an incognito tab.

This repo uses CEF(chronium extended framework) as its underlining browser engine.
The repo follows the sample cefsimple with a few tweaks. The tweaks are :

- `cef` directory is where the cef library is located for each platform. (for now only macos).
- ./Cmakelists.txt  configure the `cef` directory (macos for now).
- src/Cmakelists.txt is the main app strome. It consists of sources files for the app and handler

The repo is built with 

```
cmake -B build -G Xcode -DUSE_SANDBOX=OFF
cmake --build build --config Release --target strome
open build/src/Release/strome.app
```
