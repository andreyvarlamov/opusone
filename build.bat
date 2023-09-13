@ECHO OFF

SETLOCAL

set CommonLibDir=C:\dev\shared\libs
set CommonIncludeDir=C:\dev\shared\include

set BuildDir=%CurrProjDir%\build
set SourceDir=%CurrProjDir%\source

set CompilerOptions=/I%CommonIncludeDir% /MTd /nologo /FC /GR- /Z7 /EHa- /Od /Oi
set CompilerWarningOptions=/WX /W4 /wd4201 /wd4100 /wd4189 /wd4505
set LinkOptions=/LIBPATH:%CommonLibDir% /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:CONSOLE
set LinkLibs=SDL2main.lib SDL2.lib SDL2_image.lib glad.lib opengl32.lib shell32.lib assimp-vc143-mtd.lib

pushd %BuildDir%

cl %SourceDir%\sdl_opusone.cpp %SourceDir%\opusone.cpp %CompilerOptions% %CompilerWarningOptions% /link %LinkOptions% %LinkLibs%

popd

ENDLOCAL
