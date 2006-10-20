@echo off
set bison="c:\bison\bison++"
set flex="c:\bison\flex++"

cd library

echo.
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo NOTE! The created scanner IS NOT THREAD-SAFE unless the
echo static declarations in .cpp are moved to be class members.
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo.

echo Creating Parser..
%bison% --no-lines -oytab.cpp -d Analyse.y
if errorlevel 1 goto bisonerr
echo Creating Scanner..
%flex% -L -hlex.yy.h -olex.yy.cpp Analyse.l
if errorlevel 1 goto flexerr
goto end
:bisonerr
echo Parser creation failed
goto end
:flexerr
echo Scanner creation failed
goto end
:end
cd ..
