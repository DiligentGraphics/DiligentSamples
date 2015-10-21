To build the sample for Windows platform, open .\diligentsamples\build\Windows\Samples.sln or .\build\Windows\EngineAll.sln

To run the sample, execute one of the run_*.bat files (do not execute run.bat!)

To run the sample from the Visual Studio, set the following debugging properties:
WorkingDirectory  : $(AssetsPath)
Command Arguments : bUseOpenGL=true (or false to use DirectX, do not use spaces!)